# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'settings_resources',
      'type': 'none',
      'dependencies': [
        'device_page/compiled_resources2.gyp:*',
        'people_page/compiled_resources2.gyp:*',
        'reset_page/compiled_resources2.gyp:*',
        'settings_page/compiled_resources2.gyp:*',
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
