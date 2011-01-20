# Copyright (c) 2009 The Chromium Authors. All rights reserved.
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
      ['OS=="linux" or OS=="freebsd" or OS=="openbsd"', {'sources/': [
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
      'type': 'executable',
      'msvs_guid': 'B92AE829-E1CD-4781-824A-DCB1603A1672',
      'dependencies': [
        'ipc',
        '../base/base.gyp:base',
        '../base/base.gyp:base_i18n',
        '../base/base.gyp:test_support_base',
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
        ['OS=="linux" or OS=="freebsd" or OS=="openbsd"', {
          'dependencies': [
            '../build/linux/system.gyp:gtk',
          ],
        }],
        ['OS=="linux"', {
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
      'type': '<(library)',
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
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
