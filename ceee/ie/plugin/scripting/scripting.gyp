# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'includes': [
    '../../../../build/common.gypi',
    '../../../../ceee/common.gypi',
  ],
  'targets': [
    {
      'target_name': 'scripting',
      'type': 'static_library',
      'dependencies': [
        'javascript_bindings',
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/ceee/common/common.gyp:ceee_common',
        '<(DEPTH)/ceee/common/common.gyp:initializing_coclass',
        '<(DEPTH)/ceee/ie/common/common.gyp:ie_common',
        '<(DEPTH)/ceee/ie/common/common.gyp:ie_common_settings',
        '<(DEPTH)/ceee/ie/plugin/toolband/toolband.gyp:toolband_idl',
        '<(DEPTH)/chrome/chrome.gyp:chrome_version_header',
      ],
      'sources': [
        'base.js',
        'ceee_bootstrap.js',
        'json.js',
        'content_script_manager.cc',
        'content_script_manager.h',
        'content_script_manager.rc',
        'content_script_native_api.cc',
        'content_script_native_api.h',
        'script_host.cc',
        'script_host.h',
        'userscripts_librarian.cc',
        'userscripts_librarian.h',
        'userscripts_docs.h',
      ],
      'include_dirs': [
        # For version.h
        '<(SHARED_INTERMEDIATE_DIR)',
      ],
    },
    {
      'target_name': 'javascript_bindings',
      'type': 'none',
      'variables': {
        'chrome_renderer_path' : '../../../../chrome/renderer',
        'input_js_files': [
          '<(chrome_renderer_path)/resources/event_bindings.js',
          '<(chrome_renderer_path)/resources/renderer_extension_bindings.js',
        ],
        'output_js_files': [
          '<(SHARED_INTERMEDIATE_DIR)/event_bindings.js',
          '<(SHARED_INTERMEDIATE_DIR)/renderer_extension_bindings.js',
        ],
      },
      'sources': [
        'transform_native_js.py',
        '<@(input_js_files)',
      ],
      'actions': [
        {
          'action_name': 'transform_native_js',
          'msvs_cygwin_shell': 0,
          'msvs_quote_cmd': 0,
          'inputs': [
            '<@(_sources)',
          ],
          'outputs': [
            '<@(output_js_files)',
          ],
          'action': [
            '<@(python)',
            'transform_native_js.py',
            '<@(input_js_files)',
            '-o',
            '<(SHARED_INTERMEDIATE_DIR)',
          ],
        },
      ],
      # Make sure our dependents can refer to the transformed
      # files from their .rc file(s).
      'direct_dependent_settings': {
        'resource_include_dirs': ['<(SHARED_INTERMEDIATE_DIR)'],
      },
    },
  ]
}
