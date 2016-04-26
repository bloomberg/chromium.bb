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
      'variables': {
        'branding_path': '../../../app/theme/<(branding_path_component)/BRANDING',
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
        '..',
      ],
      'sources': [
        '<(SHARED_INTERMEDIATE_DIR)/caps/caps_version.rc',
        'exit_codes.h',
        'logger_win.cc',
        'logger_win.h',
        'main_win.cc',
        'process_singleton_win.cc',
        'process_singleton_win.h',
      ],
      'dependencies': [
        'caps_resources',
        '../../../../base/base.gyp:base',
        '../../../../components/components.gyp:generate_version_info',
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
