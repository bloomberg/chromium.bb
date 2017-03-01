# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'vr_shell_ui_api',
      'dependencies': [
        '<(EXTERNS_GYP):chrome_send',
      ],
      'includes': ['../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'vr_shell_ui_scene',
      'dependencies': [
        'vr_shell_ui_api',
      ],
      'includes': ['../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'vk',
      'dependencies': [
        'vr_shell_ui_api',
      ],
      'includes': ['../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'vr_shell_ui',
      'dependencies': [
        'vr_shell_ui_api',
        'vr_shell_ui_scene',
        'vk',
      ],
      'includes': ['../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
  ],
}
