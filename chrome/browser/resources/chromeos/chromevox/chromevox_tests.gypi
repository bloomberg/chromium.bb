# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../../../js_unittest_vars.gypi',
  ],
  'variables': {
    'chromevox_test_deps_js_file': '<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/chromeos/chromevox/test_deps.js',
  },
  'targets': [
    {
      'target_name': 'chromevox_tests',
      'type': 'executable',
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/base/base.gyp:base_i18n',
        '<(DEPTH)/base/base.gyp:test_support_base',
        '<(DEPTH)/chrome/chrome.gyp:browser',
        '<(DEPTH)/chrome/chrome.gyp:renderer',
        '<(DEPTH)/chrome/chrome.gyp:test_support_common',
        '<(DEPTH)/chrome/chrome_resources.gyp:chrome_resources',
        '<(DEPTH)/chrome/chrome_resources.gyp:chrome_strings',
        '<(DEPTH)/chrome/chrome_resources.gyp:packed_extra_resources',
        '<(DEPTH)/chrome/chrome_resources.gyp:packed_resources',
        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(DEPTH)/testing/gtest.gyp:gtest',
        '<(DEPTH)/v8/tools/gyp/v8.gyp:v8',
        'chromevox_test_deps_js',
      ],
      'defines': [
        'HAS_OUT_OF_PROC_TEST_RUNNER',
      ],
      'include_dirs': [
        '<(DEPTH)',
        '<(SHARED_INTERMEDIATE_DIR)',
      ],
      'rules': [
        {
          # A JavaScript test that runs in an environment similar to a webui
          # browser test.
          'rule_name': 'js2webui',
          'extension': 'js',
          'msvs_external_rule': 1,
          'inputs': [
            '<(gypv8sh)',
            '<(PRODUCT_DIR)/d8<(EXECUTABLE_SUFFIX)',
            '<(mock_js)',
            '<(test_api_js)',
            '<(js2gtest)',
            '<(chromevox_test_deps_js_file)',
            'testing/chromevox_unittest_base.js',
          ],
          'outputs': [
            '<(INTERMEDIATE_DIR)/<(RULE_INPUT_DIRNAME)/<(RULE_INPUT_ROOT)-gen.cc',
            '<(PRODUCT_DIR)/test_data/chrome/browser/resources/chromeos/chromevox/<(RULE_INPUT_DIRNAME)/<(RULE_INPUT_ROOT).js',
          ],
          'process_outputs_as_sources': 1,
          'action': [
            'python',
            '<(gypv8sh)',
            '<(PRODUCT_DIR)/d8<(EXECUTABLE_SUFFIX)',
            '--deps_js', '<(chromevox_test_deps_js_file)',
            '<(mock_js)',
            '<(test_api_js)',
            '<(js2gtest)',
            'webui',
            '<(RULE_INPUT_PATH)',
            'chrome/browser/resources/chromeos/chromevox/<(RULE_INPUT_DIRNAME)/<(RULE_INPUT_ROOT).js',
            '<@(_outputs)',
          ],
        },
      ],
      'sources': [
        '<(DEPTH)/chrome/browser/ui/webui/web_ui_test_handler.cc',
        '<(DEPTH)/chrome/browser/ui/webui/web_ui_test_handler.h',
        '<(DEPTH)/chrome/test/base/browser_tests_main.cc',
        '<(DEPTH)/chrome/test/base/test_chrome_web_ui_controller_factory.cc',
        '<(DEPTH)/chrome/test/base/test_chrome_web_ui_controller_factory.h',
        '<(DEPTH)/chrome/test/base/web_ui_browser_test.cc',
        '<(DEPTH)/chrome/test/base/web_ui_browser_test.h',

        'common/aria_util_test.js',
      ],
    },  # target chromevox_tests
    {
      'target_name': 'chromevox_test_deps_js',
      'type': 'none',
      'actions': [
        {
          'action_name': 'deps_js',
          'message': 'Generate <(_target_name)',
          'variables': {
            # Closure library directory relative to source tree root.
            'closure_dir': 'chrome/third_party/chromevox/third_party/closure-library/closure/goog',
            'depswriter_path': 'tools/generate_deps.py',
            'js_files': [
              '<!@(python tools/find_js_files.py . <(DEPTH)/<(closure_dir))',
            ],
          },
          'inputs': [
            '<@(js_files)',
            '<(depswriter_path)',
          ],
          'outputs': [
            '<(chromevox_test_deps_js_file)',
          ],
          'action': [
            'python',
            '<(depswriter_path)',
            '-w', '<(DEPTH)/<(closure_dir):<(closure_dir)',
            '-w', ':chrome/browser/resources/chromeos/chromevox',
            '-o', '<(chromevox_test_deps_js_file)',
            '<@(js_files)',
          ],
        },
      ],
    },
  ],
}
