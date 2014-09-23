# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'sources': [
    '<@(schema_files)',
  ],
  'variables': {
    'chromium_code': 1,
    'main_schema_files': [
      'app_current_window_internal.idl',
      'app_runtime.idl',
      'app_view_internal.json',
      'app_window.idl',
      'bluetooth.idl',
      'bluetooth_low_energy.idl',
      'bluetooth_private.json',
      'bluetooth_socket.idl',
      'cast_channel.idl',
      'dns.idl',
      'events.json',
      'extensions_manifest_types.json',
      'extension_options_internal.idl',
      'extension_types.json',
      'guest_view_internal.json',
      'hid.idl',
      'mime_handler_view_guest_internal.json',
      'power.idl',
      'runtime.json',
      'serial.idl',
      'socket.idl',
      'sockets_tcp.idl',
      'sockets_tcp_server.idl',
      'sockets_udp.idl',
      'storage.json',
      'system_cpu.idl',
      'system_display.idl',
      'system_memory.idl',
      'system_network.idl',
      'system_storage.idl',
      'test.json',
      'usb.idl',
      'usb_private.idl',
      'web_request.json',
      'web_view_internal.json',
    ],
    'non_compiled_schema_files': [
    ],
    'main_non_compiled_schema_files': [
      'web_request_internal.json',
    ],
    # TODO(thestig) Do not include this file in non-extensions builds.
    # Then the conditional and else block can go away.
    # Do the same for extensions/common/api/schemas.gni.
    'conditions': [
      ['enable_extensions==1', {
        'non_compiled_schema_files': [
          '<@(main_non_compiled_schema_files)',
        ],
        'schema_files': [
          '<@(main_schema_files)',
        ],
      }, {
        'schema_files': [
        ],
      }],
    ],
    'cc_dir': 'extensions/common/api',
    'root_namespace': 'extensions::core_api::%(namespace)s',
    'impl_dir_': 'extensions/browser/api',
  },
}
