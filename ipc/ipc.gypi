# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'target_defaults': {
    'variables': {
      'ipc_target': 0,
    },
    'target_conditions': [
      # This part is shared between the targets defined below.
      ['ipc_target==1', {
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
          'ipc_param_traits.h',
          'ipc_platform_file.h',
          'ipc_switches.cc',
          'ipc_switches.h',
          'ipc_sync_channel.cc',
          'ipc_sync_channel.h',
          'ipc_sync_message.cc',
          'ipc_sync_message.h',
          'ipc_sync_message_filter.cc',
          'ipc_sync_message_filter.h',
          'param_traits_log_macros.h',
          'param_traits_macros.h',
          'param_traits_read_macros.h',
          'param_traits_write_macros.h',
          'struct_constructor_macros.h',
          'struct_destructor_macros.h',
        ],
        'include_dirs': [
          '..',
        ],
      }],
    ],
  },
  'targets': [
    {
      'target_name': 'ipc',
      'type': '<(library)',
      'variables': {
        'ipc_target': 1,
      },
      'dependencies': [
        '../base/base.gyp:base',
      ],
      # TODO(gregoryd): direct_dependent_settings should be shared with the
      # 64-bit target, but it doesn't work due to a bug in gyp
      'direct_dependent_settings': {
        'include_dirs': [
          '..',
        ],
      },
    },
  ],
  'conditions': [
    ['OS=="win"', {
      'targets': [
        {
          'target_name': 'ipc_win64',
          'type': '<(library)',
          'variables': {
            'ipc_target': 1,
          },
          'dependencies': [
            '../base/base.gyp:base_nacl_win64',
          ],
          # TODO(gregoryd): direct_dependent_settings should be shared with the
          # 32-bit target, but it doesn't work due to a bug in gyp
          'direct_dependent_settings': {
            'include_dirs': [
              '..',
            ],
          },
          'configurations': {
            'Common_Base': {
              'msvs_target_platform': 'x64',
            },
          },
        },
      ],
    }],
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
