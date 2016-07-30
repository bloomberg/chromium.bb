# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'privacy_page_browser_proxy',
      'dependencies': [
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:cr',
        '<(EXTERNS_GYP):chrome_send',
      ],
      'includes': ['../../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'privacy_page',
      'dependencies': [
        '../compiled_resources2.gyp:route',
        '../settings_page/compiled_resources2.gyp:settings_animated_pages',
        '../settings_ui/compiled_resources2.gyp:settings_ui_types',
        '../site_settings/compiled_resources2.gyp:constants',
        'privacy_page_browser_proxy',
      ],
      'includes': ['../../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
  ],
}
