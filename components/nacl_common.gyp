# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'nacl_switches',
      'type': 'static_library',
      'sources': [
        'nacl/common/nacl_switches.cc',
        'nacl/common/nacl_switches.h',
      ],
      'include_dirs': [
        '..',
      ],
    },
    {
      'target_name': 'nacl_common',
      'type': 'static_library',
      'sources': [
        'nacl/common/nacl_cmd_line.cc',
        'nacl/common/nacl_cmd_line.h',
        'nacl/common/nacl_messages.cc',
        'nacl/common/nacl_messages.h',
        'nacl/common/nacl_types.cc',
        'nacl/common/nacl_types.h',
      ],
      'include_dirs': [
        '..',
      ],
    },
  ],
  'conditions': [
    ['OS=="win" and target_arch=="ia32"', {
      'targets': [
        {
          'target_name': 'nacl_switches_win64',
          'type': 'static_library',
          'sources': [
            'nacl/common/nacl_switches.cc',
            'nacl/common/nacl_switches.h',
          ],
          'include_dirs': [
            '..',
          ],
          'configurations': {
            'Common_Base': {
              'msvs_target_platform': 'x64',
            },
          },
        },
        {
          'target_name': 'nacl_common_win64',
          'type': 'static_library',
          'sources': [
            'nacl/common/nacl_cmd_line.cc',
            'nacl/common/nacl_cmd_line.h',
            'nacl/common/nacl_messages.cc',
            'nacl/common/nacl_messages.h',
            'nacl/common/nacl_types.cc',
            'nacl/common/nacl_types.h',
          ],
          'include_dirs': [
            '..',
          ],
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
