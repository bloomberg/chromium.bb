# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'uber_utils',
      'variables': {
        'depends': [
          '../../../../ui/webui/resources/js/cr.js',
          '../../../../ui/webui/resources/js/compiled_resources.gyp:load_time_data',
          '../../../../ui/webui/resources/js/util.js',
        ],
        'externs': ['<(EXTERNS_DIR)/chrome_send.js'],
      },
      'includes': ['../../../../third_party/closure_compiler/compile_js.gypi'],
    },
    {
      'target_name': 'uber',
      'variables': {
        'depends': [
          '../../../../ui/webui/resources/js/cr.js',
          '../../../../ui/webui/resources/js/compiled_resources.gyp:load_time_data',
          '../../../../ui/webui/resources/js/util.js',
          'uber_utils.js',
        ],
        'externs': ['<(EXTERNS_DIR)/chrome_send.js'],
      },
      'includes': ['../../../../third_party/closure_compiler/compile_js.gypi'],
    },
  ],
}
