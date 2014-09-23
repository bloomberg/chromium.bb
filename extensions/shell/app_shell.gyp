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
        '<(DEPTH)/components/components.gyp:web_cache_renderer',
        '<(DEPTH)/content/content.gyp:content',
        '<(DEPTH)/content/content.gyp:content_gpu',
        '<(DEPTH)/content/content.gyp:content_ppapi_plugin',
        '<(DEPTH)/content/content_shell_and_tests.gyp:content_shell_lib',
        '<(DEPTH)/device/core/core.gyp:device_core',
        '<(DEPTH)/device/hid/hid.gyp:device_hid',
        '<(DEPTH)/extensions/browser/api/api_registration.gyp:extensions_api_registration',
        '<(DEPTH)/extensions/common/api/api.gyp:extensions_api',
        '<(DEPTH)/extensions/extensions.gyp:extensions_browser',
        '<(DEPTH)/extensions/extensions.gyp:extensions_common',
        '<(DEPTH)/extensions/extensions.gyp:extensions_renderer',
        '<(DEPTH)/extensions/extensions.gyp:extensions_shell_and_test_pak',
        '<(DEPTH)/extensions/extensions_resources.gyp:extensions_resources',
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
        'browser/default_shell_browser_main_delegate.cc',
        'browser/default_shell_browser_main_delegate.h',
        'browser/desktop_controller.cc',
        'browser/desktop_controller.h',
        'browser/media_capture_util.cc',
        'browser/media_capture_util.h',
        'browser/shell_app_delegate.cc',
        'browser/shell_app_delegate.h',
        'browser/shell_app_window_client.cc',
        'browser/shell_app_window_client.h',
        'browser/shell_audio_controller_chromeos.cc',
        'browser/shell_audio_controller_chromeos.h',
        'browser/shell_browser_context.cc',
        'browser/shell_browser_context.h',
        'browser/shell_browser_main_delegate.h',
        'browser/shell_browser_main_parts.cc',
        'browser/shell_browser_main_parts.h',
        'browser/shell_content_browser_client.cc',
        'browser/shell_content_browser_client.h',
        'browser/shell_desktop_controller.cc',
        'browser/shell_desktop_controller.h',
        'browser/shell_device_client.cc',
        'browser/shell_device_client.h',
        'browser/shell_display_info_provider.cc',
        'browser/shell_display_info_provider.h',
        'browser/shell_extension_host_delegate.cc',
        'browser/shell_extension_host_delegate.h',
        'browser/shell_extension_system.cc',
        'browser/shell_extension_system.h',
        'browser/shell_extension_system_factory.cc',
        'browser/shell_extension_system_factory.h',
        'browser/shell_extension_web_contents_observer.cc',
        'browser/shell_extension_web_contents_observer.h',
        'browser/shell_extensions_browser_client.cc',
        'browser/shell_extensions_browser_client.h',
        'browser/shell_native_app_window.cc',
        'browser/shell_native_app_window.h',
        'browser/shell_network_controller_chromeos.cc',
        'browser/shell_network_controller_chromeos.h',
        'browser/shell_omaha_query_params_delegate.cc',
        'browser/shell_omaha_query_params_delegate.h',
        'browser/shell_runtime_api_delegate.cc',
        'browser/shell_runtime_api_delegate.h',
        'browser/shell_special_storage_policy.cc',
        'browser/shell_special_storage_policy.h',
        'browser/shell_web_contents_modal_dialog_manager.cc',
        'common/shell_content_client.cc',
        'common/shell_content_client.h',
        'common/shell_extensions_client.cc',
        'common/shell_extensions_client.h',
        'common/switches.h',
        'common/switches.cc',
        'renderer/shell_content_renderer_client.cc',
        'renderer/shell_content_renderer_client.h',
        'renderer/shell_extensions_renderer_client.cc',
        'renderer/shell_extensions_renderer_client.h',
      ],
      'conditions': [
        ['chromeos==1', {
          'dependencies': [
            '<(DEPTH)/chromeos/chromeos.gyp:chromeos',
            '<(DEPTH)/ui/chromeos/ui_chromeos.gyp:ui_chromeos',
            '<(DEPTH)/ui/display/display.gyp:display',
          ],
        }],
        ['disable_nacl==0 and OS=="linux"', {
          'dependencies': [
            '<(DEPTH)/components/nacl.gyp:nacl_helper',
          ],
        }],
        ['disable_nacl==0', {
          'dependencies': [
            '<(DEPTH)/components/nacl.gyp:nacl',
            '<(DEPTH)/components/nacl.gyp:nacl_browser',
            '<(DEPTH)/components/nacl.gyp:nacl_common',
            '<(DEPTH)/components/nacl.gyp:nacl_renderer',
            '<(DEPTH)/components/nacl.gyp:nacl_switches',
          ],
          'sources': [
            'browser/shell_nacl_browser_delegate.cc',
            'browser/shell_nacl_browser_delegate.h',
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
        '../browser/guest_view/web_view/web_view_apitest.cc',
        'browser/shell_browsertest.cc',
        'test/shell_apitest.cc',
        'test/shell_apitest.h',
        'test/shell_test.cc',
        'test/shell_test.h',
        'test/shell_test_launcher_delegate.cc',
        'test/shell_test_launcher_delegate.h',
        'test/shell_tests_main.cc',
      ],
    },
    {
      'target_name': 'app_shell_unittests',
      'type': 'executable',
      'dependencies': [
        'app_shell_lib',
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/base/base.gyp:test_support_base',
        '<(DEPTH)/content/content.gyp:content_app_both',
        '<(DEPTH)/content/content_shell_and_tests.gyp:test_support_content',
        '<(DEPTH)/extensions/extensions.gyp:extensions_shell_and_test_pak',
        '<(DEPTH)/extensions/extensions.gyp:extensions_test_support',
        '<(DEPTH)/testing/gtest.gyp:gtest',
        '<(DEPTH)/ui/aura/aura.gyp:aura_test_support',
      ],
      'sources': [
        '../test/extensions_unittests_main.cc',
        'browser/shell_audio_controller_chromeos_unittest.cc',
        'browser/shell_desktop_controller_unittest.cc',
        'browser/shell_nacl_browser_delegate_unittest.cc',
        'common/shell_content_client_unittest.cc'
      ],
      'conditions': [
        ['disable_nacl==1', {
          'sources!': [
            'browser/shell_nacl_browser_delegate_unittest.cc',
          ],
        }],
        ['chromeos==1', {
          'dependencies': [
            '<(DEPTH)/chromeos/chromeos.gyp:chromeos_test_support_without_gmock',
          ],
        }],
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
          'variables': {
            'lastchange_path': '<(DEPTH)/build/util/LASTCHANGE',
          },
          'inputs': [
            '<(version_path)',
            '<(lastchange_path)',
            'common/version.h.in',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/extensions/shell/common/version.h',
          ],
          'action': [
            'python',
            '<(version_py_path)',
            '-e', 'VERSION_FULL="<(version_full)"',
            '-f', '<(lastchange_path)',
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
