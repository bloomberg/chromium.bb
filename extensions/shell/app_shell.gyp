# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'app_shell_lib',
      'type': 'static_library',
      'defines!': ['CONTENT_IMPLEMENTATION'],
      'dependencies': [
        'app_shell_version_header',
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/base/base.gyp:base_prefs_test_support',
        '<(DEPTH)/components/components.gyp:omaha_query_params',
        '<(DEPTH)/components/components.gyp:pref_registry',
        '<(DEPTH)/components/components.gyp:user_prefs',
        '<(DEPTH)/content/content.gyp:content',
        '<(DEPTH)/content/content.gyp:content_gpu',
        '<(DEPTH)/content/content.gyp:content_ppapi_plugin',
        '<(DEPTH)/content/content_shell_and_tests.gyp:content_shell_lib',
        '<(DEPTH)/device/hid/hid.gyp:device_hid',
        '<(DEPTH)/extensions/common/api/api.gyp:extensions_api',
        '<(DEPTH)/extensions/extensions.gyp:extensions_browser',
        '<(DEPTH)/extensions/extensions.gyp:extensions_common',
        '<(DEPTH)/extensions/extensions.gyp:extensions_renderer',
        '<(DEPTH)/extensions/extensions.gyp:extensions_shell_and_test_pak',
        '<(DEPTH)/extensions/extensions_resources.gyp:extensions_resources',
        '<(DEPTH)/extensions/shell/common/api/api.gyp:shell_api',
        '<(DEPTH)/mojo/mojo_base.gyp:mojo_environment_chromium',
        '<(DEPTH)/mojo/mojo_base.gyp:mojo_system_impl',
        '<(DEPTH)/skia/skia.gyp:skia',
        '<(DEPTH)/third_party/WebKit/public/blink.gyp:blink',
        '<(DEPTH)/ui/wm/wm.gyp:wm',
        '<(DEPTH)/v8/tools/gyp/v8.gyp:v8',
      ],
      'include_dirs': [
        '../..',
        '<(SHARED_INTERMEDIATE_DIR)',
        '<(SHARED_INTERMEDIATE_DIR)/extensions/shell',
      ],
      'sources': [
        'app/shell_main_delegate.cc',
        'app/shell_main_delegate.h',
        'browser/api/shell/shell_api.cc',
        'browser/api/shell/shell_api.h',
        'browser/api/shell_extensions_api_client.cc',
        'browser/api/shell_extensions_api_client.h',
        'browser/default_shell_browser_main_delegate.cc',
        'browser/default_shell_browser_main_delegate.h',
        'browser/default_shell_app_window_controller.cc',
        'browser/default_shell_app_window_controller.h',
        'browser/shell_app_sorting.cc',
        'browser/shell_app_sorting.h',
        'browser/shell_app_window.cc',
        'browser/shell_app_window.h',
        'browser/shell_app_window_controller.h',
        'browser/shell_browser_context.cc',
        'browser/shell_browser_context.h',
        'browser/shell_browser_main_delegate.h',
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
        'browser/shell_network_controller_chromeos.cc',
        'browser/shell_network_controller_chromeos.h',
        'browser/shell_omaha_query_params_delegate.cc',
        'browser/shell_omaha_query_params_delegate.h',
        'browser/shell_runtime_api_delegate.cc',
        'browser/shell_runtime_api_delegate.h',
        'browser/shell_special_storage_policy.cc',
        'browser/shell_special_storage_policy.h',
        'common/shell_content_client.cc',
        'common/shell_content_client.h',
        'common/shell_extensions_client.cc',
        'common/shell_extensions_client.h',
        'common/switches.h',
        'common/switches.cc',
        'renderer/shell_content_renderer_client.cc',
        'renderer/shell_content_renderer_client.h',
        'renderer/shell_custom_bindings.cc',
        'renderer/shell_custom_bindings.h',
        'renderer/shell_custom_bindings.js',
        'renderer/shell_dispatcher_delegate.cc',
        'renderer/shell_dispatcher_delegate.h',
        'renderer/shell_extensions_renderer_client.cc',
        'renderer/shell_extensions_renderer_client.h',
        'renderer/shell_renderer_main_delegate.h',
      ],
      'conditions': [
        ['chromeos==1', {
          'dependencies': [
            '<(DEPTH)/chromeos/chromeos.gyp:chromeos',
            '<(DEPTH)/ui/chromeos/ui_chromeos.gyp:ui_chromeos',
            '<(DEPTH)/ui/display/display.gyp:display',
          ],
        }],
      ],
    },
    {
      'target_name': 'app_shell',
      'type': 'executable',
      'defines!': ['CONTENT_IMPLEMENTATION'],
      'dependencies': [
        'app_shell_lib',
        '<(DEPTH)/extensions/extensions.gyp:extensions_shell_and_test_pak',
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
        '<(DEPTH)/extensions/extensions.gyp:extensions_test_support',
        '<(DEPTH)/testing/gtest.gyp:gtest',
      ],
      'defines': [
        'HAS_OUT_OF_PROC_TEST_RUNNER',
      ],
      'sources': [
        # TODO(yoz): Refactor once we have a second test target.
        # TODO(yoz): Something is off here; should this .gyp file be
        # in the parent directory? Test target extensions_browsertests?
        '../browser/api/dns/dns_apitest.cc',
        '../browser/api/socket/socket_apitest.cc',
        '../browser/api/sockets_tcp/sockets_tcp_apitest.cc',
        '../browser/api/sockets_udp/sockets_udp_apitest.cc',
        'browser/shell_browsertest.cc',
        'test/shell_test.h',
        'test/shell_test.cc',
        'test/shell_test_launcher_delegate.cc',
        'test/shell_test_launcher_delegate.h',
        'test/shell_tests_main.cc',
      ],
    },
    {
      'target_name': 'app_shell_version_header',
      'type': 'none',
      'direct_dependent_settings': {
        'include_dirs': [
          '<(SHARED_INTERMEDIATE_DIR)',
        ],
      },
      'actions': [
        {
          'action_name': 'version_header',
          'message': 'Generating version header file: <@(_outputs)',
          'inputs': [
            '<(version_path)',
            'common/version.h.in',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/extensions/shell/common/version.h',
          ],
          'action': [
            'python',
            '<(version_py_path)',
            '-e', 'VERSION_FULL="<(version_full)"',
            'common/version.h.in',
            '<@(_outputs)',
          ],
          'includes': [
            '../../build/util/version.gypi',
          ],
        },
      ],
    },
  ],  # targets
}
