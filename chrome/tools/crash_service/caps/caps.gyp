# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'variables': {
    'chromium_code': 1,
  },
  'includes': [
    '../../../../build/util/version.gypi',
    '../../../../build/win_precompile.gypi',
  ],
  'targets': [
    {
      'target_name': 'caps_resources',
      'type': 'none',
      'conditions': [
        ['branding == "Chrome"', {
          'variables': {
             'branding_path': '../../../app/theme/google_chrome/BRANDING',
          },
        }, { # else branding!="Chrome"
          'variables': {
             'branding_path': '../../../app/theme/chromium/BRANDING',
          },
        }],
      ],
      'variables': {
        'output_dir': 'caps',
        'template_input_path': '../../../app/chrome_version.rc.version',
      },
      'sources': [
        'caps.ver',
      ],
      'includes': [
        '../../../version_resource_rules.gypi',
      ],
    },
    {
      'target_name': 'caps',
      'type': 'executable',
      'include_dirs': [
        '../..',
      ],
      'sources': [
        'main_win.cc',
        '<(SHARED_INTERMEDIATE_DIR)/caps/caps_version.rc',
      ],
      'dependencies': [
        'caps_resources',
      ],
      'msvs_settings': {
        'VCLinkerTool': {
          # Set /SUBSYSTEM:WINDOWS.
          'SubSystem': '2',
        },
      },
    },
  ],
}
