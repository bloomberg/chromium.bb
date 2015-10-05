# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'settings_checkbox',
      'variables': {
        'depends': [
          '../policy_controllable/policy_controllable.js',
        ],
        'externs': [
          '../../../../../third_party/closure_compiler/externs/settings_private.js'
        ],
      },
      'includes': ['../../../../../third_party/closure_compiler/compile_js.gypi'],
    },
    {
      'target_name': 'settings_input',
      'variables': {
        'depends': [
          '../../../../../ui/webui/resources/js/compiled_resources.gyp:assert',
          '../policy_controllable/policy_controllable.js',
        ],
        'externs': [
          '../../../../../third_party/closure_compiler/externs/settings_private.js'
        ],
      },
      'includes': ['../../../../../third_party/closure_compiler/compile_js.gypi'],
    },
    {
      'target_name': 'settings_radio_group',
      'variables': {
        'depends': [
          '../../../../../ui/webui/resources/js/compiled_resources.gyp:assert',
        ],
        'externs': [
          '../../../../../third_party/closure_compiler/externs/settings_private.js'
        ],
      },
      'includes': ['../../../../../third_party/closure_compiler/compile_js.gypi'],
    },
  ],
}
