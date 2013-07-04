# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'target_defaults': {
    'variables': {
      'breakpad_component_target': 0,
    },
    'target_conditions': [
      ['breakpad_component_target==1', {
        'sources': [
          'breakpad/breakpad_client.cc',
          'breakpad/breakpad_client.h',
        ],
      }],
    ],
  },
  'targets': [
    {
      'target_name': 'breakpad_component',
      'type': 'static_library',
      'variables': {
        'breakpad_component_target': 1,
      },
      'dependencies': [
        '../base/base.gyp:base',
      ],
    },
  ],
  'conditions': [
    ['OS=="win" and target_arch=="ia32"', {
      'targets': [
        {
          'target_name': 'breakpad_win64',
          'type': 'static_library',
          'variables': {
            'breakpad_component_target': 1,
          },
          'dependencies': [
            '../base/base.gyp:base_nacl_win64',
          ],
          'configurations': {
            'Common_Base': {
              'msvs_target_platform': 'x64',
            },
          },
        },
      ],
    }],
    ['OS=="mac"', {
      'targets': [
        {
          # TODO(jochen): for now, this target is a copy of breakpad, however,
          # in the future, it should provide a dummy implementation for Mac.
          'target_name': 'breakpad_stubs',
          'type': 'static_library',
          'variables': {
            'breakpad_component_target': 1,
          },
          'dependencies': [
            '../base/base.gyp:base',
          ],
        },
      ],
    }],
  ],
}
