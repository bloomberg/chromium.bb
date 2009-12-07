# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'target_defaults': {
    'sources/': [
      ['exclude', '/win/'],
      ['exclude', '_(posix|win)(_unittest)?\\.(cc|mm?)$'],
      ['exclude', '/win_[^/]*\\.cc$'],
    ],
    'conditions': [
      ['OS=="linux"', {'sources/': [
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
        '../testing/gtest.gyp:gtest',
      ],
      'include_dirs': [
        '..'
      ],
      'sources': [
        'file_descriptor_set_posix_unittest.cc',
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
        ['OS=="linux"', {
          'dependencies': [
            '../build/linux/system.gyp:gtk',
          ],
        }],
        ['OS=="linux" and toolkit_views==1', {
          'dependencies': [
            '../views/views.gyp:views',
          ],
        }],
      ],
    },
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
