# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'extensions_api',
      'type': 'static_library',
      'sources': [
        '<@(schema_files)',
      ],
      'includes': [
        '../../../build/json_schema_bundle_compile.gypi',
        '../../../build/json_schema_compile.gypi',
      ],
      'variables': {
        'chromium_code': 1,
        'non_compiled_schema_files': [
        ],
        'schema_files': [
          'socket.idl',
          'sockets_tcp.idl',
          'sockets_tcp_server.idl',
          'sockets_udp.idl',
        ],
        'cc_dir': 'extensions/common/api',
        'root_namespace': 'extensions::core_api',
        'impl_dir': 'extensions/browser/api',
      },
      'dependencies': [
        '<(DEPTH)/skia/skia.gyp:skia',
      ],
    },
  ],
}
