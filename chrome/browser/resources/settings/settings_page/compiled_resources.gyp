# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'settings_page_visibility',
      'includes': ['../../../../../third_party/closure_compiler/compile_js.gypi'],
    },
    {
      'target_name': 'settings_router',
      'variables': {
        'depends': [
          '../../../../../ui/webui/resources/js/compiled_resources.gyp:assert',
          '../../../../../ui/webui/resources/js/compiled_resources.gyp:load_time_data',
        ],
      },
      'includes': ['../../../../../third_party/closure_compiler/compile_js.gypi'],
    },
  ],
}
