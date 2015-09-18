# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'on_startup_page',
      'variables': {
        'depends': [
          '../../../../../ui/webui/resources/js/compiled_resources.gyp:assert',
          '../../../../../ui/webui/resources/js/compiled_resources.gyp:load_time_data',
          '../settings_page/settings_animated_pages.js'
        ],
        'externs': [
          '../../../../../third_party/closure_compiler/externs/settings_private.js'
        ],
      },
      'includes': ['../../../../../third_party/closure_compiler/compile_js.gypi'],
    },
    {
      'target_name': 'startup_urls_page',
      'variables': {
        'depends': [
          '../../../../../ui/webui/resources/js/compiled_resources.gyp:load_time_data',
          '../../../../../ui/webui/resources/js/compiled_resources.gyp:cr',
        ],
        'externs': [
          '../../../../../third_party/closure_compiler/externs/settings_private.js'
        ],
      },
      'includes': ['../../../../../third_party/closure_compiler/compile_js.gypi'],
    },
  ],
}
