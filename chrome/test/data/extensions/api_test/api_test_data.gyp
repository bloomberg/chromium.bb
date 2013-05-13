# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../../../../ppapi/ppapi_nacl_test_common.gypi',
  ],
  'targets': [
    {
      'target_name': 'socket_ppapi',
      'type': 'none',
      'variables': {
        'nexe_target': 'extensions_api_test_socket_ppapi',
        'build_newlib': 1,
        'build_glibc': 0,
        'build_pnacl_newlib': 0,
        'nexe_destination_dir': 'test_data/chrome/test/data/extensions/api_test/socket/ppapi',
        'sources': [
          'socket/ppapi/test_socket.cc',
          '<(DEPTH)/ppapi/tests/test_utils.cc',
          '<(DEPTH)/ppapi/tests/test_utils.h',
        ],
        'test_files': [
          'socket/ppapi/controller.js',
          'socket/ppapi/index.html',
          'socket/ppapi/main.js',
          'socket/ppapi/manifest.json',
        ],
      },
    },
  ],
}
