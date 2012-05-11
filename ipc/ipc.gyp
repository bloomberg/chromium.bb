# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'target_defaults': {
    'sources/': [
      ['exclude', '/win/'],
      ['exclude', '_(posix|win)(_unittest)?\\.(cc|mm?)$'],
      ['exclude', '/win_[^/]*\\.cc$'],
    ],
    'conditions': [
      ['os_posix == 1 and OS != "mac"', {'sources/': [
        ['include', '_posix(_unittest)?\\.cc$'],
      ]}],
      ['OS=="mac"', {'sources/': [
        ['include', '_posix(_unittest)?\\.(cc|mm?)$'],
      ]}],
      ['OS=="win"', {'sources/': [
        ['include', '_win(_unittest)?\\.cc$'],
        ['include', '/win/'],
        ['include', '/win_[^/]*\\.cc$'],
      ]}],
    ],
  },
  'includes': [
    'ipc.gypi',
  ],
  'targets': [
    {
      'target_name': 'ipc_tests',
      'type': '<(gtest_target_type)',
      'dependencies': [
        'ipc',
        '../base/base.gyp:base',
        '../base/base.gyp:base_i18n',
        '../base/base.gyp:test_support_base',
        '../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '../testing/gtest.gyp:gtest',
      ],
      'include_dirs': [
        '..'
      ],
      'sources': [
        'file_descriptor_set_posix_unittest.cc',
        'ipc_channel_posix_unittest.cc',
        'ipc_fuzzing_tests.cc',
        'ipc_message_unittest.cc',
        'ipc_send_fds_test.cc',
        'ipc_sync_channel_unittest.cc',
        'ipc_sync_message_unittest.cc',
        'ipc_sync_message_unittest.h',
        'ipc_tests.cc',
        'ipc_tests.h',
        'sync_socket_unittest.cc',
      ],
      'conditions': [
        ['toolkit_uses_gtk == 1', {
          'dependencies': [
            '../build/linux/system.gyp:gtk',
          ],
        }],
        ['OS=="android" and "<(gtest_target_type)"=="shared_library"', {
          'dependencies': [
            '../testing/android/native_test.gyp:native_test_native_code',
          ],
        }],
        ['os_posix == 1 and OS != "mac" and OS != "android"', {
          'conditions': [
            ['linux_use_tcmalloc==1', {
              'dependencies': [
                '../base/allocator/allocator.gyp:allocator',
              ],
            }],
          ],
        }]
      ],
    },
    {
      'target_name': 'test_support_ipc',
      'type': 'static_library',
      'dependencies': [
        'ipc',
        '../base/base.gyp:base',
      ],
      'sources': [
        'ipc_test_sink.cc',
        'ipc_test_sink.h',
      ],
    },
  ],
  'conditions': [
    # Special target to wrap a <(gtest_target_type)==shared_library
    # ipc_tests into an android apk for execution.
    # See base.gyp for TODO(jrg)s about this strategy.
    ['OS=="android" and "<(gtest_target_type)"=="shared_library"', {
      'targets': [
        {
          'target_name': 'ipc_tests_apk',
          'type': 'none',
          'dependencies': [
            'ipc_tests',
          ],
          'actions': [
            {
              # Generate apk files (including source and antfile) from
              # a template, and builds them.
              'action_name': 'generate_and_build',
              'inputs': [
                '../testing/android/generate_native_test.py',
                '<(PRODUCT_DIR)/lib.target/libipc_tests.so',
                '<(PRODUCT_DIR)/lib.java/chromium_base.jar'
              ],
              'outputs': [
                '<(PRODUCT_DIR)/ChromeNativeTests_ipc_tests-debug.apk',
              ],
              'action': [ 
                '../testing/android/generate_native_test.py',
                '--native_library',
                '<(PRODUCT_DIR)/lib.target/libipc_tests.so',
                # TODO(jrg): find a better way to specify jar
                # dependencies.  Hard coding seems fragile.
                '--jar',
                '<(PRODUCT_DIR)/lib.java/chromium_base.jar',
                '--output',
                '<(PRODUCT_DIR)/ipc_tests_apk',
                '--ant-args', 
                '-DPRODUCT_DIR=<(PRODUCT_DIR)',
                '--ant-compile'
              ],
            },
          ],
        }],
    }],
  ],
}
