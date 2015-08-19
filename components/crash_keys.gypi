# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/crash_keys
      'target_name': 'crash_keys',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        # List of dependencies is intentionally very minimal. Please avoid
        # adding extra dependencies without first checking with OWNERS.
        '../base/base.gyp:base',
      ],
      'sources': [
        'crash_keys/crash_keys.cc',
        'crash_keys/crash_keys.h',
      ],
    },
  ],
  'conditions': [
    ['OS=="win" and target_arch=="ia32"', {
      'targets': [
        {
          'target_name': 'crash_keys_win64',
          'type': 'static_library',
          'sources': [
            'crash_keys/crash_keys.cc',
            'crash_keys/crash_keys.h',
          ],
          'dependencies': [
            '../base/base.gyp:base_win64',
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
