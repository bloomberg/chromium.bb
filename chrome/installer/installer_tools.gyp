# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'version_py': '<(DEPTH)/build/util/version.py',
    'version_path': '<(DEPTH)/chrome/VERSION',
    'lastchange_path': '<(DEPTH)/build/util/LASTCHANGE',
    'branding_dir': '<(DEPTH)/chrome/app/theme/<(branding_path_component)',
    'msvs_use_common_release': 0,
    'msvs_use_common_linker_extras': 0,
  },
  'conditions': [
    ['OS=="win"', {
      'targets': [
        {
          'target_name': 'validate_installation',
          'type': 'executable',
          'dependencies': [
            '<(DEPTH)/base/base.gyp:base',
            '<(DEPTH)/chrome/chrome.gyp:installer_util',
            '<(DEPTH)/chrome/chrome.gyp:installer_util_strings',
            '<(DEPTH)/chrome/common_constants.gyp:common_constants',
          ],
          'include_dirs': [
            '<(DEPTH)',
          ],
          'sources': [
            'tools/validate_installation.rc',
            'tools/validate_installation_main.cc',
            'tools/validate_installation_resource.h',
          ],
        },
        {
          # A target that is outdated if any of the mini_installer test sources
          # are modified.
          'target_name': 'test_installer_sentinel',
          'type': 'none',
          'includes': [
            '../test/mini_installer/test_installer.gypi',
          ],
          'actions': [
            {
              'action_name': 'touch_sentinel',
              'variables': {
                'touch_sentinel_py': '../tools/build/win/touch_sentinel.py',
              },
              'inputs': [
                '<@(test_installer_sources)',  # from test_installer.gypi
                '<(touch_sentinel_py)',
              ],
              'outputs': [
                '<(SHARED_INTERMEDIATE_DIR)/chrome/installer/test_installer_sentinel',
              ],
              'action': ['python', '<(touch_sentinel_py)', '<@(_outputs)'],
            },
          ],
        },
      ],
    }],
  ],
}
