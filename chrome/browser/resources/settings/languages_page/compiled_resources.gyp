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
        ],
        'externs': [
          '<(EXTERNS_DIR)/chrome_send.js',
          '../../../../../third_party/closure_compiler/externs/language_settings_private.js'
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
          'languages.js',
        ],
        'externs': [
          '../../../../../third_party/closure_compiler/externs/language_settings_private.js',
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
          'languages.js',
        ],
        'externs': [
          '../../../../../third_party/closure_compiler/externs/language_settings_private.js',
        ],
      },
      'includes': ['../../../../../third_party/closure_compiler/compile_js.gypi'],
    },
  ],
}
