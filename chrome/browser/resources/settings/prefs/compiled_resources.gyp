# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'prefs',
      'variables': {
        'depends': [
          '../../../../../third_party/closure_compiler/externs/settings_private_interface.js',
          '../../../../../ui/webui/resources/js/compiled_resources.gyp:assert',
          '../../../../../ui/webui/resources/js/compiled_resources.gyp:cr',
          'pref_util.js',
          'prefs_behavior.js',
          'prefs_types.js',
        ],
        'externs': [
          '../../../../../third_party/closure_compiler/externs/settings_private.js'
        ],
      },
      'includes': ['../../../../../third_party/closure_compiler/compile_js.gypi'],
    },
  ],
}
