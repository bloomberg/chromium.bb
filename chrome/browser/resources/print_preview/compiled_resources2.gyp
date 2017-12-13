# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'print_preview',
      'dependencies': [
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:cr',
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:event_tracker',
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:webui_listener_tracker',
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:load_time_data',
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:promise_resolver',
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:util',
        '<(DEPTH)/ui/webui/resources/js/cr/compiled_resources2.gyp:event_target',
        '<(DEPTH)/ui/webui/resources/js/cr/ui/compiled_resources2.gyp:focus_manager',
        '<(DEPTH)/ui/webui/resources/js/cr/ui/compiled_resources2.gyp:focus_outline_manager',
        '<(DEPTH)/ui/webui/resources/js/cr/ui/compiled_resources2.gyp:node_utils',
        '<(DEPTH)/ui/webui/resources/js/cr/compiled_resources2.gyp:ui',
        '<(EXTERNS_GYP):chrome_send',
      ],
      'variables': {
        'extra_inputs': [
          '<!@(python <(CLOSURE_DIR)/build/get_includes.py print_preview.js)',
        ],
      },
      'includes': ['../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'print_preview_utils',
      'dependencies': [
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:cr',
        'data/compiled_resources2.gyp:size',
        'data/compiled_resources2.gyp:coordinate2d'
      ],
      'includes': ['../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'metrics',
      'dependencies': [
        'native_layer',
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:cr',
      ],
      'includes': ['../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'cloud_print_interface',
      'dependencies': [
        'native_layer',
        'data/compiled_resources2.gyp:cloud_parsers',
        'data/compiled_resources2.gyp:destination',
        'data/compiled_resources2.gyp:document_info',
        'data/compiled_resources2.gyp:invitation',
        'data/compiled_resources2.gyp:user_info',
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:cr',
      ],
      'includes': ['../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'native_layer',
      'dependencies': [
        'data/compiled_resources2.gyp:destination',
        'data/compiled_resources2.gyp:measurement_system',
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:cr',
      ],
      'includes': ['../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
  ],
}
