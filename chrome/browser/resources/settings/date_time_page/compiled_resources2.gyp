# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'date_time_page',
      'dependencies': [
        '../prefs/compiled_resources2.gyp:prefs_behavior',
        '<(DEPTH)/third_party/polymer/v1_0/components-chromium/paper-checkbox/compiled_resources2.gyp:paper-checkbox-extracted',
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:cr',
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:load_time_data',
      ],
      'includes': ['../../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
  ],
}
