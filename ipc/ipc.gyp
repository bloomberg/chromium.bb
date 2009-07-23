# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../build/common.gypi',
  ],
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
  'targets': [
   {
      'target_name': 'ipc',
      'type': '<(library)',
      'dependencies': [
        '../base/base.gyp:base',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'file_descriptor_set_posix.cc',
        'file_descriptor_set_posix.h',
        'ipc_channel.h',
        'ipc_channel_handle.h',
        'ipc_channel_posix.cc',
        'ipc_channel_posix.h',
        'ipc_channel_proxy.cc',
        'ipc_channel_proxy.h',
        'ipc_channel_win.cc',
        'ipc_channel_win.h',
        'ipc_descriptors.h',
        'ipc_logging.cc',
        'ipc_logging.h',
        'ipc_message.cc',
        'ipc_message.h',
        'ipc_message_macros.h',
        'ipc_message_utils.cc',
        'ipc_message_utils.h',
        'ipc_switches.cc',
        'ipc_switches.h',
        'ipc_sync_channel.cc',
        'ipc_sync_channel.h',
        'ipc_sync_message.cc',
        'ipc_sync_message.h',
     ],
     'direct_dependent_settings': {
        'include_dirs': [
          '..',
        ],
      },
    },
    {
      'target_name': 'ipc_tests',
      'type': 'executable',
      'msvs_guid': 'B92AE829-E1CD-4781-824A-DCB1603A1672',
      'dependencies': [
        'ipc',
        '../base/base.gyp:base',
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
  ]
}
