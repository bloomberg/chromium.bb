# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'variables': {
    'chromium_code': 1,
  },
  'includes': [
    '../../build/util/version.gypi',
    '../../build/win_precompile.gypi',
  ],
  'targets': [
    {
      'target_name': 'chrome_watcher_resources',
      'type': 'none',
      'conditions': [
        ['branding == "Chrome"', {
          'variables': {
             'branding_path': '../app/theme/google_chrome/BRANDING',
          },
        }, { # else branding!="Chrome"
          'variables': {
             'branding_path': '../app/theme/chromium/BRANDING',
          },
        }],
      ],
      'variables': {
        'output_dir': '.',
        'template_input_path': '../app/chrome_version.rc.version',
      },
      'sources': [
        'chrome_watcher.ver',
      ],
      'includes': [
        '../version_resource_rules.gypi',
      ],
    },
    {
      'target_name': 'chrome_watcher',
      'type': 'loadable_module',
      'include_dirs': [
        '../..',
      ],
      'sources': [
        'chrome_watcher.def',
        'chrome_watcher_main.cc',
        '<(SHARED_INTERMEDIATE_DIR)/chrome_watcher/chrome_watcher_version.rc',
      ],
      'dependencies': [
        'chrome_watcher_resources',
        '../components/components.gyp:browser_watcher',
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
