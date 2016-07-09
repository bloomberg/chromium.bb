# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'includes': [
    'command_buffer.gypi',
  ],
  'targets': [
    {
      'target_name': 'gles2_utils',
      'type': '<(component)',
      'variables': {
        'gles2_utils_target': 1,
      },
      'dependencies': [
        '../../base/base.gyp:base',
        '../../ui/gfx/gfx.gyp:gfx_geometry',
        '../../third_party/khronos/khronos.gyp:khronos_headers',
      ],
      'export_dependent_settings': [
        '../../base/base.gyp:base',
      ],
    },
  ],
  'conditions': [
    ['disable_nacl!=1 and OS=="win" and target_arch=="ia32"', {
      'targets': [
        {
          'target_name': 'gles2_utils_win64',
          'type': '<(component)',
          'variables': {
            'gles2_utils_target': 1,
          },
          'dependencies': [
            '../../base/base.gyp:base_win64',
            '../../ui/gfx/gfx.gyp:gfx_geometry_win64',
            '../../third_party/khronos/khronos.gyp:khronos_headers',
          ],
          'export_dependent_settings': [
            '../../base/base.gyp:base_win64',
          ],
          'defines': [
            '<@(nacl_win64_defines)',
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

