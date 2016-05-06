# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'settings_resources',
      'type': 'none',
      'dependencies': [
        'about_page/compiled_resources2.gyp:*',
        'advanced_page/compiled_resources2.gyp:*',
        'appearance_page/compiled_resources2.gyp:*',
        'basic_page/compiled_resources2.gyp:*',
        'bluetooth_page/compiled_resources2.gyp:*',
        'controls/compiled_resources2.gyp:*',
        'device_page/compiled_resources2.gyp:*',
        'internet_page/compiled_resources2.gyp:*',
        'languages_page/compiled_resources2.gyp:*',
        'on_startup_page/compiled_resources2.gyp:*',
        'passwords_and_forms_page/compiled_resources2.gyp:*',
        'people_page/compiled_resources2.gyp:*',
        'prefs/compiled_resources2.gyp:*',
        'reset_page/compiled_resources2.gyp:*',
        'settings_page/compiled_resources2.gyp:*',
        'site_settings/compiled_resources2.gyp:*',
        'site_settings_page/compiled_resources2.gyp:*',
        'system_page/compiled_resources2.gyp:*',
      ],
    },
    {
      'target_name': 'direction_delegate',
      'dependencies': [
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:assert',
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:cr',
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:load_time_data',
      ],
      'includes': ['../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
  ],
}
