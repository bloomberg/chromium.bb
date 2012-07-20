# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  ######################################################################
  'includes': [
    '../../../../../native_client/build/common.gypi',
  ],
  ######################################################################
  'conditions': [
    ['target_arch!="arm"', {
      'targets': [
        {
          'target_name': 'file_helper_nexe',
          'type': 'none',
          'dependencies': [
            '<(DEPTH)/native_client/tools.gyp:prep_toolchain',
            '<(DEPTH)/native_client/src/untrusted/nacl/nacl.gyp:nacl_lib',
            '<(DEPTH)/native_client/src/untrusted/irt/irt.gyp:irt_core_nexe'
          ],
          'variables': {
            'nexe_target': 'file_helper',
            'build_glibc': 0,
            'build_newlib': 1,
            'extra_args': [
              '--strip-debug',
            ],
          },
          'sources': [
            'plugin/file_helper.cc',
          ],
          'link_flags': [
          '-lppapi_cpp',
            '-lppapi',
            '-lplatform',
            '-lpthread',
            '-lgio',
          ],
          'copies': [
            {
              'destination': '<(PRODUCT_DIR)/test_data/chrome/extensions/file_helper',
              'files': [
                'extension/event.js',
                'extension/file_helper_newlib.nmf',
                'extension/manifest.json',
                'extension/test.html',
                'extension/test.js',
                '<(PRODUCT_DIR)/file_helper_newlib_x32.nexe',
                '<(PRODUCT_DIR)/file_helper_newlib_x64.nexe'
              ],
          },
        ],
        },
      ],
    }],
  ],
}
