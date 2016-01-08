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
      # Users of the watcher link this target.
      'target_name': 'chrome_watcher_client',
      'type': 'static_library',
      'sources': [
        'chrome_watcher_main_api.cc',
        'chrome_watcher_main_api.h',
      ],
      'dependencies': [
        '../base/base.gyp:base',
      ],
    },
    {
      'target_name': 'chrome_watcher',
      'type': 'loadable_module',
      'include_dirs': [
        '../..',
      ],
      'sources': [
        '<(SHARED_INTERMEDIATE_DIR)/chrome_watcher/chrome_watcher_version.rc',
        'chrome_watcher.def',
        'chrome_watcher_main.cc',
      ],
      'dependencies': [
        'chrome_watcher_client',
        'chrome_watcher_resources',
        'installer_util',
        '../base/base.gyp:base',
        '../components/components.gyp:browser_watcher',
        '../third_party/kasko/kasko.gyp:kasko',
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
