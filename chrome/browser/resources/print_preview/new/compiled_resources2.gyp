# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'app',
      'dependencies': [
        'header',
        'destination_settings',
        'pages_settings',
        'copies_settings',
        'layout_settings',
        'color_settings',
        'media_size_settings',
        'margins_settings',
        'dpi_settings',
        'scaling_settings',
        'other_options_settings',
        'advanced_options_settings',
        'preview_area',
        'model',
        'state',
        '../compiled_resources2.gyp:native_layer',
        '../data/compiled_resources2.gyp:destination',
        '../data/compiled_resources2.gyp:destination_store',
        '../data/compiled_resources2.gyp:document_info',
        '../data/compiled_resources2.gyp:measurement_system',
        '../data/compiled_resources2.gyp:user_info',
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:event_tracker',
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:webui_listener_tracker',
      ],
      'includes': ['../../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'header',
      'dependencies': [
        '../data/compiled_resources2.gyp:destination',
        'model',
        'settings_behavior',
        'state',
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:load_time_data',
      ],
      'includes': ['../../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'destination_settings',
      'dependencies': [
        '../data/compiled_resources2.gyp:destination',
      ],
      'includes': ['../../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'pages_settings',
      'dependencies': [
        'settings_behavior',
        '../data/compiled_resources2.gyp:document_info',
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:cr',
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:load_time_data',
      ],
      'includes': ['../../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'copies_settings',
      'dependencies': [
        'number_settings_section',
        'settings_behavior',
      ],
      'includes': ['../../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'layout_settings',
      'dependencies': [
        'settings_behavior',
      ],
      'includes': ['../../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'color_settings',
      'dependencies': [
        'settings_behavior',
      ],
      'includes': ['../../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'media_size_settings',
      'dependencies': [
        'settings_behavior',
        'settings_select',
      ],
      'includes': ['../../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'margins_settings',
      'dependencies': [
        'settings_behavior',
      ],
      'includes': ['../../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'dpi_settings',
      'dependencies': [
        'settings_behavior',
        'settings_select',
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:cr',
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:load_time_data',
      ],
      'includes': ['../../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'scaling_settings',
      'dependencies': [
        '../data/compiled_resources2.gyp:document_info',
        'number_settings_section',
        'settings_behavior',
      ],
      'includes': ['../../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'other_options_settings',
      'dependencies': [
        'settings_behavior',
      ],
      'includes': ['../../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'advanced_options_settings',
      'dependencies': [
      ],
      'includes': ['../../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'number_settings_section',
      'dependencies': [
      ],
      'includes': ['../../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'settings_select',
      'dependencies': [
        'settings_behavior',
        '../compiled_resources2.gyp:print_preview_utils',
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:cr',
      ],
      'includes': ['../../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'settings_behavior',
      'dependencies': [
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:assert',
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:cr',
      ],
      'includes': ['../../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'preview_area',
      'dependencies': [
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:cr',
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:web_ui_listener_behavior',
        '../../pdf/compiled_resources2.gyp:pdf_scripting_api',
        '../compiled_resources2.gyp:native_layer',
        '../compiled_resources2.gyp:print_preview_utils',
        '../data/compiled_resources2.gyp:destination',
        '../data/compiled_resources2.gyp:document_info',
        '../data/compiled_resources2.gyp:coordinate2d',
        '../data/compiled_resources2.gyp:size',
        '../data/compiled_resources2.gyp:margins',
        '../data/compiled_resources2.gyp:printable_area',
        'settings_behavior',
        'state',
      ],
      'includes': ['../../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'model',
      'dependencies': [
        'settings_behavior',
        '../data/compiled_resources2.gyp:destination',
        '../data/compiled_resources2.gyp:document_info',
        '../data/compiled_resources2.gyp:margins',
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:cr',
      ],
      'includes': ['../../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'state',
      'dependencies': [
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:cr',
      ],
      'includes': ['../../../../../third_party/closure_compiler/compile_js2.gypi'],
    }
  ],
}
