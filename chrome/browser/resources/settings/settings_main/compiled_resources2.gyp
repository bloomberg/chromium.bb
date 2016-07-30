# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'settings_main',
      'dependencies': [
        '../compiled_resources2.gyp:route',
        '../compiled_resources2.gyp:search_settings',
        '../settings_page/compiled_resources2.gyp:main_page_behavior',
        '../settings_page/compiled_resources2.gyp:settings_router',
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:assert',
        '../settings_page/compiled_resources2.gyp:settings_page_visibility',
        '../settings_ui/compiled_resources2.gyp:settings_ui_types',
      ],
      'includes': ['../../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
  ],
}
