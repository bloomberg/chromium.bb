# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'bluetooth_page',
      'variables': {
        'depends': [
          '../../../../../third_party/closure_compiler/externs/bluetooth_interface.js',
          '../../../../../third_party/closure_compiler/externs/bluetooth_private_interface.js',
          '../../../../../ui/webui/resources/js/compiled_resources.gyp:assert',
          '../../../../../ui/webui/resources/js/compiled_resources.gyp:load_time_data',
          '../../../../../ui/webui/resources/js/i18n_behavior.js',
          '../settings_page/settings_animated_pages.js'
        ],
        'externs': [
          '../../../../../third_party/closure_compiler/externs/bluetooth.js',
          '../../../../../third_party/closure_compiler/externs/bluetooth_private.js'
        ],
      },
      'includes': ['../../../../../third_party/closure_compiler/compile_js.gypi'],
    },
    {
      'target_name': 'bluetooth_device_list_item',
      'variables': {
        'depends': [
          '../../../../../ui/webui/resources/js/compiled_resources.gyp:assert',
        ],
        'externs': [
          '../../../../../third_party/closure_compiler/externs/bluetooth.js',
          '../../../../../third_party/closure_compiler/externs/bluetooth_private.js'
        ],
      },
      'includes': ['../../../../../third_party/closure_compiler/compile_js.gypi'],
    },
    {
      'target_name': 'bluetooth_add_device_dialog',
      'variables': {
        'depends': [
          '../../../../../third_party/closure_compiler/externs/bluetooth_interface.js',
          '../../../../../third_party/closure_compiler/externs/bluetooth_private_interface.js',
          '../../../../../ui/webui/resources/js/compiled_resources.gyp:assert',
          '../../../../../ui/webui/resources/js/compiled_resources.gyp:load_time_data',
          '../../../../../ui/webui/resources/js/i18n_behavior.js',
        ],
        'externs': [
          '../../../../../third_party/closure_compiler/externs/bluetooth.js',
          '../../../../../third_party/closure_compiler/externs/bluetooth_private.js'
        ],
      },
      'includes': ['../../../../../third_party/closure_compiler/compile_js.gypi'],
    },
    {
      'target_name': 'bluetooth_pair_device_dialog',
      'variables': {
        'depends': [
          '../../../../../third_party/closure_compiler/externs/bluetooth_interface.js',
          '../../../../../third_party/closure_compiler/externs/bluetooth_private_interface.js',
          '../../../../../ui/webui/resources/js/compiled_resources.gyp:assert',
          '../../../../../ui/webui/resources/js/compiled_resources.gyp:load_time_data',
          '../../../../../ui/webui/resources/js/i18n_behavior.js',
        ],
        'externs': [
          '../../../../../third_party/closure_compiler/externs/bluetooth.js',
          '../../../../../third_party/closure_compiler/externs/bluetooth_private.js'
        ],
      },
      'includes': ['../../../../../third_party/closure_compiler/compile_js.gypi'],
    },
  ],
}
