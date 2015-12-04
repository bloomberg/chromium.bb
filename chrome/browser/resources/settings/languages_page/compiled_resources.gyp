# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'languages',
      'variables': {
        'depends': [
          '../prefs/prefs_types.js',
          '../prefs/compiled_resources.gyp:prefs',
          '../../../../../ui/webui/resources/js/compiled_resources.gyp:assert',
          '../../../../../ui/webui/resources/js/compiled_resources.gyp:cr',
          'languages_types.js',
        ],
        'externs': [
          '<(EXTERNS_DIR)/chrome_send.js',
          '<(EXTERNS_DIR)/language_settings_private.js',
        ],
      },
      'includes': ['../../../../../third_party/closure_compiler/compile_js.gypi'],
    },
    {
      'target_name': 'language_detail_page',
      'variables': {
        'depends': [
          '../../../../../ui/webui/resources/cr_elements/policy/compiled_resources.gyp:cr_policy_indicator_behavior',
          '../../../../../ui/webui/resources/js/chromeos/compiled_resources.gyp:ui_account_tweaks',
          '../../../../../ui/webui/resources/js/compiled_resources.gyp:load_time_data',
          '../prefs/compiled_resources.gyp:prefs',
          'languages_types.js',
          'languages.js',
        ],
        'externs': [
          '<(EXTERNS_DIR)/language_settings_private.js',
        ],
      },
      'includes': ['../../../../../third_party/closure_compiler/compile_js.gypi'],
    },
    {
      'target_name': 'languages_page',
      'variables': {
        'depends': [
          '../../../../../ui/webui/resources/js/compiled_resources.gyp:assert',
          '../../../../../ui/webui/resources/js/compiled_resources.gyp:cr',
          '../../../../../ui/webui/resources/js/compiled_resources.gyp:load_time_data',
          '../settings_page/settings_animated_pages.js',
          '../prefs/prefs_types.js',
          '../prefs/compiled_resources.gyp:prefs',
          'languages_types.js',
          'languages.js',
        ],
        'externs': [
          '<(EXTERNS_DIR)/language_settings_private.js',
        ],
      },
      'includes': ['../../../../../third_party/closure_compiler/compile_js.gypi'],
    },
    {
      'target_name': 'manage_languages_page',
      'variables': {
        'depends': [
          '../../../../../ui/webui/resources/js/compiled_resources.gyp:assert',
          '../prefs/compiled_resources.gyp:prefs',
          'languages_types.js',
          'languages.js',
        ],
        'externs': [
          '<(EXTERNS_DIR)/language_settings_private.js',
        ],
      },
      'includes': ['../../../../../third_party/closure_compiler/compile_js.gypi'],
    },
  ],
}
