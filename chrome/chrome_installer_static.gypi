# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# The install_static_util target should only depend on functions in kernel32
# and advapi32. Please don't add dependencies on other system libraries.
{
  'target_defaults': {
    'variables': {
      'installer_static_target': 0,
    },
    'target_conditions': [
      # This part is shared between the two versions of the target.
      ['installer_static_target==1', {
        'sources': [
          'install_static/install_util.h',
          'install_static/install_util.cc',
        ],
        'include_dirs': [
          '<(DEPTH)',
        ],
      }],
    ],
  },
  'conditions': [
    ['OS=="win"', {
      'targets': [
        {
          # GN version: //chrome/install_static
          'target_name': 'install_static_util',
          'type': 'static_library',
          'variables': {
            'installer_static_target': 1,
          },
          'dependencies': [
            'installer_util_strings',
            '<(DEPTH)/base/base.gyp:base',
          ],
          'msvs_settings': {
            'VCLinkerTool': {
              # Please don't add dependencies on other system libraries.
              'AdditionalDependencies': [
                'kernel32.lib',
                'advapi32.lib',
              ],
            },
          },
        },
      ],
    }],
    ['OS=="win" and target_arch=="ia32"', {
      'targets': [
        {
          'target_name': 'install_static_util_nacl_win64',
          'type': 'static_library',
          'variables': {
            'installer_static_target': 1,
          },
          'dependencies': [
            'installer_util_strings',
          ],
          'include_dirs': [
            '<(SHARED_INTERMEDIATE_DIR)',
          ],
          'configurations': {
            'Common_Base': {
              'msvs_target_platform': 'x64',
            },
          },
          'msvs_settings': {
            'VCLinkerTool': {
              # Please don't add dependencies on other system libraries.
              'AdditionalDependencies': [
                'kernel32.lib',
                'advapi32.lib',
              ],
            },
          },
        },
      ],
    }],
  ],
}
