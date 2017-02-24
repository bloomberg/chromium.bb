# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'app',
      'dependencies': [
        '<(EXTERNS_GYP):chrome_extensions',
        'store'
      ],
      'includes': ['../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'folder_node',
      'includes': ['../../../../third_party/closure_compiler/compile_js2.gypi'],
      'dependencies': [
        '<(EXTERNS_GYP):chrome_extensions',
        'store'
      ],
    },
    {
      'target_name': 'item',
      'dependencies': [
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:icon',
        '<(EXTERNS_GYP):chrome_extensions',
      ],
      'includes': ['../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'list',
      'dependencies': [
        '<(DEPTH)/ui/webui/resources/cr_elements/cr_action_menu/compiled_resources2.gyp:cr_action_menu',
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:load_time_data',
	'<(EXTERNS_GYP):bookmark_manager_private',
	'<(EXTERNS_GYP):chrome_extensions',
        'item',
      ],
      'includes': ['../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'router',
      'includes': ['../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'sidebar',
      'dependencies': [
        'folder_node',
      ],
      'includes': ['../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'store',
      'dependencies': [
        '<(EXTERNS_GYP):chrome_extensions',
        'router',
      ],
      'includes': ['../../../../third_party/closure_compiler/compile_js2.gypi']
    },
    {
      'target_name': 'toolbar',
      'dependencies': [
        '<(DEPTH)/ui/webui/resources/cr_elements/cr_action_menu/compiled_resources2.gyp:cr_action_menu',
        '<(DEPTH)/ui/webui/resources/cr_elements/cr_toolbar/compiled_resources2.gyp:cr_toolbar',
        '<(EXTERNS_GYP):chrome_extensions',
      ],
      'includes': ['../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
  ]
}
