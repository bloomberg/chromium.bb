# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'app_shell_pak',
      'type': 'none',
      'dependencies': [
        # Need extension related resources in common_resources.pak and
        # renderer_resources_100_percent.pak
        '<(DEPTH)/chrome/chrome_resources.gyp:chrome_resources',
        # Need app related resources in theme_resources_100_percent.pak
        '<(DEPTH)/chrome/chrome_resources.gyp:theme_resources',
        # Need dev-tools related resources in shell_resources.pak and
        # devtools_resources.pak.
        '<(DEPTH)/content/content_shell_and_tests.gyp:generate_content_shell_resources',
        '<(DEPTH)/content/browser/devtools/devtools_resources.gyp:devtools_resources',
        '<(DEPTH)/ui/base/strings/ui_strings.gyp:ui_strings',
        '<(DEPTH)/ui/resources/ui_resources.gyp:ui_resources',
      ],
      'actions': [
        {
          'action_name': 'repack_app_shell_pack',
          'variables': {
            'pak_inputs': [
              '<(SHARED_INTERMEDIATE_DIR)/chrome/common_resources.pak',
              '<(SHARED_INTERMEDIATE_DIR)/chrome/extensions_api_resources.pak',
              # TODO(jamescook): Extract the extension/app related resources
              # from generated_resources_en-US.pak and
              # theme_resources_100_percent.pak.
              '<(SHARED_INTERMEDIATE_DIR)/chrome/generated_resources_en-US.pak',
              '<(SHARED_INTERMEDIATE_DIR)/chrome/renderer_resources_100_percent.pak',
              '<(SHARED_INTERMEDIATE_DIR)/chrome/theme_resources_100_percent.pak',
              '<(SHARED_INTERMEDIATE_DIR)/content/shell_resources.pak',
              '<(SHARED_INTERMEDIATE_DIR)/ui/app_locale_settings/app_locale_settings_en-US.pak',
              '<(SHARED_INTERMEDIATE_DIR)/ui/ui_resources/ui_resources_100_percent.pak',
              '<(SHARED_INTERMEDIATE_DIR)/ui/ui_strings/ui_strings_en-US.pak',
              '<(SHARED_INTERMEDIATE_DIR)/webkit/devtools_resources.pak',
            ],
            'pak_output': '<(PRODUCT_DIR)/app_shell.pak',
          },
         'includes': [ '../../build/repack_action.gypi' ],
        },
      ],
    },
    {
      'target_name': 'app_shell_lib',
      'type': 'static_library',
      'defines!': ['CONTENT_IMPLEMENTATION'],
      'dependencies': [
        '<(DEPTH)/chrome/chrome.gyp:browser',
        '<(DEPTH)/chrome/chrome.gyp:browser_extensions',
        '<(DEPTH)/chrome/chrome.gyp:debugger',
        '<(DEPTH)/chrome/chrome.gyp:plugin',
        '<(DEPTH)/chrome/chrome.gyp:renderer',
        '<(DEPTH)/chrome/chrome.gyp:utility',
        '<(DEPTH)/chrome/common/extensions/api/api.gyp:chrome_api',
        '<(DEPTH)/third_party/WebKit/public/blink_devtools.gyp:blink_devtools_frontend_resources',
        # TODO(rockot): Dependencies above this line are temporary.
        # See http://crbug.com/359656.
        'app_shell_pak',
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/base/base.gyp:base_prefs_test_support',
        '<(DEPTH)/content/content.gyp:content',
        '<(DEPTH)/content/content.gyp:content_gpu',
        '<(DEPTH)/content/content.gyp:content_ppapi_plugin',
        '<(DEPTH)/content/content.gyp:content_worker',
        '<(DEPTH)/content/content_shell_and_tests.gyp:content_shell_lib',
        '<(DEPTH)/extensions/common/api/api.gyp:extensions_api',
        '<(DEPTH)/extensions/extensions.gyp:extensions_browser',
        '<(DEPTH)/extensions/extensions.gyp:extensions_common',
        '<(DEPTH)/extensions/extensions.gyp:extensions_renderer',
        '<(DEPTH)/skia/skia.gyp:skia',
        '<(DEPTH)/third_party/WebKit/public/blink.gyp:blink',
        '<(DEPTH)/ui/wm/wm.gyp:wm_test_support',
        '<(DEPTH)/v8/tools/gyp/v8.gyp:v8',
      ],
      'include_dirs': [
        '../..',
        '<(SHARED_INTERMEDIATE_DIR)',
      ],
      'sources': [
        'app/shell_main_delegate.cc',
        'app/shell_main_delegate.h',
        'browser/shell_app_runtime_api.cc',
        'browser/shell_app_runtime_api.h',
        'browser/shell_app_sorting.cc',
        'browser/shell_app_sorting.h',
        'browser/shell_app_window.cc',
        'browser/shell_app_window.h',
        'browser/shell_app_window_api.cc',
        'browser/shell_app_window_api.h',
        'browser/shell_browser_context.cc',
        'browser/shell_browser_context.h',
        'browser/shell_browser_main_parts.cc',
        'browser/shell_browser_main_parts.h',
        'browser/shell_content_browser_client.cc',
        'browser/shell_content_browser_client.h',
        'browser/shell_desktop_controller.cc',
        'browser/shell_desktop_controller.h',
        'browser/shell_extension_system.cc',
        'browser/shell_extension_system.h',
        'browser/shell_extension_system_factory.cc',
        'browser/shell_extension_system_factory.h',
        'browser/shell_extension_web_contents_observer.cc',
        'browser/shell_extension_web_contents_observer.h',
        'browser/shell_extensions_browser_client.cc',
        'browser/shell_extensions_browser_client.h',
        'common/shell_content_client.cc',
        'common/shell_content_client.h',
        'common/shell_extensions_client.cc',
        'common/shell_extensions_client.h',
        'renderer/shell_content_renderer_client.cc',
        'renderer/shell_content_renderer_client.h',
      ],
    },
    {
      'target_name': 'app_shell',
      'type': 'executable',
      'defines!': ['CONTENT_IMPLEMENTATION'],
      'dependencies': [
        'app_shell_lib',
        'app_shell_pak',
      ],
      'include_dirs': [
        '../..',
      ],
      'sources': [
        'app/shell_main.cc',
      ],
      'conditions': [
        ['OS=="win"', {
          'msvs_settings': {
            'VCLinkerTool': {
              'SubSystem': '2',  # Set /SUBSYSTEM:WINDOWS
            },
          },
          'msvs_large_pdb': 1,
          'dependencies': [
            '<(DEPTH)/sandbox/sandbox.gyp:sandbox',
          ],
        }],
      ],
    },
    {
      'target_name': 'app_shell_browsertests',
      'type': '<(gtest_target_type)',
      'dependencies': [
        'app_shell_lib',
        # TODO(yoz): find the right deps
        '<(DEPTH)/base/base.gyp:test_support_base',
        '<(DEPTH)/content/content.gyp:content_app_both',
        '<(DEPTH)/content/content_shell_and_tests.gyp:content_browser_test_support',
        '<(DEPTH)/content/content_shell_and_tests.gyp:test_support_content',
        '<(DEPTH)/testing/gtest.gyp:gtest',
      ],
      'defines': [
        'HAS_OUT_OF_PROC_TEST_RUNNER',
      ],
      'msvs_large_pdb': 1,
      'sources': [
        # TODO(yoz): Refactor once we have a second test target.
        '../test/app_shell_test.h',
        '../test/app_shell_test.cc',
        '../test/apps_test_launcher_delegate.cc',
        '../test/apps_test_launcher_delegate.h',
        '../test/apps_tests_main.cc',
      ],
    },
  ],  # targets
}
