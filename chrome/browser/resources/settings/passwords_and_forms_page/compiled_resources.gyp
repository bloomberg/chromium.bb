# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'passwords_and_forms_page',
      'variables': {
        'depends': [
          'compiled_resources.gyp:passwords_section',
          '../../../../../third_party/closure_compiler/externs/compiled_resources.gyp:passwords_private',
        ],
      },
      'includes': ['../../../../../third_party/closure_compiler/compile_js.gypi'],
    },
    {
      'target_name': 'passwords_section',
      'includes': ['../../../../../third_party/closure_compiler/compile_js.gypi'],
      'variables': {
        'depends': [
          '../../../../../third_party/closure_compiler/externs/compiled_resources.gyp:passwords_private',
        ],
      },
    },
  ],
}
