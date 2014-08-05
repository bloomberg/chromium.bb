# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //extensions/common/api
      'target_name': 'extensions_api',
      'type': 'static_library',
      'sources': [
        '<@(schema_files)',
      ],
      # TODO(jschuh): http://crbug.com/167187 size_t -> int
      'msvs_disabled_warnings': [ 4267 ],
      'includes': [
        '../../../build/json_schema_bundle_compile.gypi',
        '../../../build/json_schema_compile.gypi',
      ],
      'variables': {
        'chromium_code': 1,
        'non_compiled_schema_files': [
        ],
        'conditions': [
          ['enable_extensions==1', {
            # Note: file list duplicated in GN build.
            'schema_files': [
              'app_runtime.idl',
              'app_view_internal.json',
              'cast_channel.idl',
              'dns.idl',
              'extensions_manifest_types.json',
              'hid.idl',
              'power.idl',
              'runtime.json',
              'serial.idl',
              'socket.idl',
              'sockets_tcp.idl',
              'sockets_tcp_server.idl',
              'sockets_udp.idl',
              'storage.json',
              'test.json',
              'usb.idl',
            ],
          }, {
            # TODO: Eliminate these on Android. See crbug.com/305852.
            'schema_files': [
              'runtime.json',
            ],
          }],
        ],
        'cc_dir': 'extensions/common/api',
        'root_namespace': 'extensions::core_api::%(namespace)s',
        'impl_dir': 'extensions/browser/api',
      },
      'conditions': [
        ['enable_extensions==1', {
          'dependencies': [
            '<(DEPTH)/device/serial/serial.gyp:device_serial',
            '<(DEPTH)/skia/skia.gyp:skia',
          ],
        }],
      ],
    },
  ],
}
