# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'manager',
      'variables': {
        'depends': [
          '../../../../ui/webui/resources/cr_elements/cr_search_field/cr_search_field.js',
          '../../../../ui/webui/resources/js/assert.js',
          '../../../../ui/webui/resources/js/cr.js',
        ],
        'externs': [
          '<(EXTERNS_DIR)/chrome_send.js',
        ],
      },
      'includes': ['../../../../third_party/closure_compiler/compile_js.gypi'],
    }
  ],
}
