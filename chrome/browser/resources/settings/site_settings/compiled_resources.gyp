# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'site_details',
      'variables': {
        'depends': [
          '../../../../../ui/webui/resources/js/compiled_resources.gyp:assert',
          '../../../../../ui/webui/resources/js/compiled_resources.gyp:cr',
          '../../../../../ui/webui/resources/js/compiled_resources.gyp:load_time_data',
          '../prefs/prefs_behavior.js',
          'constants.js',
          'site_settings_behavior.js',
          'site_details_permission.js',
          'website_usage_private_api.js',
        ],
        'externs': [
          '../../../../../third_party/closure_compiler/externs/settings_private.js',
        ],
      },
      'includes': ['../../../../../third_party/closure_compiler/compile_js.gypi'],
    },
    {
      'target_name': 'site_details_permission',
      'variables': {
        'depends': [
          '../../../../../ui/webui/resources/js/compiled_resources.gyp:assert',
          '../../../../../ui/webui/resources/js/compiled_resources.gyp:cr',
          '../../../../../ui/webui/resources/js/compiled_resources.gyp:load_time_data',
          '../prefs/prefs_behavior.js',
          'constants.js',
          'site_settings_behavior.js',
        ],
        'externs': [
          '../../../../../third_party/closure_compiler/externs/settings_private.js',
        ],
      },
      'includes': ['../../../../../third_party/closure_compiler/compile_js.gypi'],
    },
    {
      'target_name': 'site_list',
      'variables': {
        'depends': [
          '../../../../../ui/webui/resources/js/compiled_resources.gyp:assert',
          '../../../../../ui/webui/resources/js/compiled_resources.gyp:cr',
          '../../../../../ui/webui/resources/js/compiled_resources.gyp:load_time_data',
          '../prefs/prefs_behavior.js',
          '../prefs/prefs_types.js',
          'constants.js',
          'site_settings_behavior.js',
        ],
        'externs': [
          '../../../../../third_party/closure_compiler/externs/settings_private.js',
        ],
      },
      'includes': ['../../../../../third_party/closure_compiler/compile_js.gypi'],
    },
    {
      'target_name': 'site_settings_behavior',
      'variables': {
        'depends': [
          '../../../../../ui/webui/resources/js/compiled_resources.gyp:assert',
          '../../../../../ui/webui/resources/js/compiled_resources.gyp:cr',
          '../../../../../ui/webui/resources/js/compiled_resources.gyp:load_time_data',
          '../prefs/prefs_behavior.js',
          'constants.js',
        ],
        'externs': [
          '../../../../../third_party/closure_compiler/externs/settings_private.js',
        ],
      },
      'includes': ['../../../../../third_party/closure_compiler/compile_js.gypi'],
    },
    {
      'target_name': 'site_settings_category',
      'variables': {
        'depends': [
          '../../../../../ui/webui/resources/js/compiled_resources.gyp:assert',
          '../../../../../ui/webui/resources/js/compiled_resources.gyp:cr',
          '../../../../../ui/webui/resources/js/compiled_resources.gyp:load_time_data',
          '../prefs/prefs_behavior.js',
          '../prefs/prefs_types.js',
          '../settings_page/settings_animated_pages.js',
          'constants.js',
          'site_settings_behavior.js',
        ],
        'externs': [
          '../../../../../third_party/closure_compiler/externs/settings_private.js',
        ],
      },
      'includes': ['../../../../../third_party/closure_compiler/compile_js.gypi'],
    },
  ],
}
