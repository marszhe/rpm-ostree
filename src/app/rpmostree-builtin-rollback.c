/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2014 Colin Walters <walters@verbum.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2 of the licence or (at
 * your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include <string.h>
#include <glib-unix.h>

#include "rpmostree-builtins.h"
#include "rpmostree-libbuiltin.h"
#include "rpmostree-dbus-helpers.h"

#include <libglnx.h>

static gboolean opt_reboot;

static GOptionEntry option_entries[] = {
  { "reboot", 'r', 0, G_OPTION_ARG_NONE, &opt_reboot, "Initiate a reboot after rollback is prepared", NULL },
  { NULL }
};

static GVariant *
get_args_variant (void)
{
  GVariantDict dict;

  g_variant_dict_init (&dict, NULL);
  g_variant_dict_insert (&dict, "reboot", "b", opt_reboot);

  return g_variant_dict_end (&dict);
}

int
rpmostree_builtin_rollback (int             argc,
                            char          **argv,
                            RpmOstreeCommandInvocation *invocation,
                            GCancellable   *cancellable,
                            GError        **error)
{
  GOptionContext *context = g_option_context_new ("- Revert to the previously booted tree");
  glnx_unref_object RPMOSTreeOS *os_proxy = NULL;
  glnx_unref_object RPMOSTreeSysroot *sysroot_proxy = NULL;
  g_autofree char *transaction_address = NULL;
  _cleanup_peer_ GPid peer_pid = 0;

  if (!rpmostree_option_context_parse (context,
                                       option_entries,
                                       &argc, &argv,
                                       invocation,
                                       cancellable,
                                       NULL, NULL,
                                       &sysroot_proxy,
                                       &peer_pid,
                                       error))
    return EXIT_FAILURE;

  if (!rpmostree_load_os_proxy (sysroot_proxy, NULL,
                                cancellable, &os_proxy, error))
    return EXIT_FAILURE;

  if (!rpmostree_os_call_rollback_sync (os_proxy,
                                        get_args_variant (),
                                        &transaction_address,
                                        cancellable,
                                        error))
    return EXIT_FAILURE;

  if (!rpmostree_transaction_get_response_sync (sysroot_proxy,
                                                transaction_address,
                                                cancellable,
                                                error))
    return EXIT_FAILURE;

  if (!opt_reboot)
    {
      const char *sysroot_path;

      sysroot_path = rpmostree_sysroot_get_path (sysroot_proxy);

      /* By request, doing this without dbus */
      if (!rpmostree_print_treepkg_diff_from_sysroot_path (sysroot_path,
                                                           cancellable,
                                                           error))
        return EXIT_FAILURE;

      g_print ("Run \"systemctl reboot\" to start a reboot\n");
    }


  return EXIT_SUCCESS;
}
