# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'basic_page',
      'dependencies': [
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:load_time_data',
        '../compiled_resources2.gyp:route',
        '../compiled_resources2.gyp:search_settings',
        '../settings_page/compiled_resources2.gyp:main_page_behavior',
        '../settings_page/compiled_resources2.gyp:settings_page_visibility',
        '../settings_ui/compiled_resources2.gyp:settings_ui_types',
      ],
      'includes': ['../../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
  ],
}
