# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'multidevice_setup',
      'dependencies': [
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:cr',
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:i18n_behavior',
        'button_navigation_behavior',
        'start_setup_page',
        'setting_up_page',
        'setup_succeeded_page',
        'setup_failed_page',
      ],
      'includes': ['../../../../../third_party/closure_compiler/compile_js2.gypi'],
    },

    {
      'target_name': 'button_navigation_behavior',
      'dependencies': [],
      'includes': ['../../../../../third_party/closure_compiler/compile_js2.gypi'],
    },

    {
      'target_name': 'start_setup_page',
      'dependencies': ['button_navigation_behavior'],
      'includes': ['../../../../../third_party/closure_compiler/compile_js2.gypi'],
    },

    {
      'target_name': 'setting_up_page',
      'dependencies': ['button_navigation_behavior'],
      'includes': ['../../../../../third_party/closure_compiler/compile_js2.gypi'],
    },

    {
      'target_name': 'setup_succeeded_page',
      'dependencies': ['button_navigation_behavior'],
      'includes': ['../../../../../third_party/closure_compiler/compile_js2.gypi'],
    },

    {
      'target_name': 'setup_failed_page',
      'dependencies': ['button_navigation_behavior'],
      'includes': ['../../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
  ],
}
