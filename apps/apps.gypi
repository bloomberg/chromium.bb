# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'apps',
      'type': 'static_library',
      'variables': { 'enable_wexit_time_destructors': 1, },
      # Since browser and browser_extensions actually depend on each other,
      # we must omit the dependency from browser_extensions to browser.
      # However, this means browser_extensions and browser should more or less
      # have the same dependencies. Once browser_extensions is untangled from
      # browser, then we can clean up these dependencies.
      'dependencies': [
        'browser_extensions',
        'common/extensions/api/api.gyp:chrome_api',
        '../apps/common/api/api.gyp:apps_api',
        '../skia/skia.gyp:skia',
      ],
      'include_dirs': [
        '<(INTERMEDIATE_DIR)',
        '<(grit_out_dir)',
      ],
      'sources': [
        'app_lifetime_monitor.cc',
        'app_lifetime_monitor.h',
        'app_lifetime_monitor_factory.cc',
        'app_lifetime_monitor_factory.h',
        'app_load_service.cc',
        'app_load_service.h',
        'app_load_service_factory.cc',
        'app_load_service_factory.h',
        'app_restore_service.cc',
        'app_restore_service.h',
        'app_restore_service_factory.cc',
        'app_restore_service_factory.h',
        'app_window.cc',
        'app_window.h',
        'app_window_contents.cc',
        'app_window_contents.h',
        'app_window_geometry_cache.cc',
        'app_window_geometry_cache.h',
        'app_window_registry.cc',
        'app_window_registry.h',
        'apps_client.cc',
        'apps_client.h',
        'browser_context_keyed_service_factories.cc',
        'browser_context_keyed_service_factories.h',
        'browser/api/app_runtime/app_runtime_api.cc',
        'browser/api/app_runtime/app_runtime_api.h',
        'browser/file_handler_util.cc',
        'browser/file_handler_util.h',
        'launcher.cc',
        'launcher.h',
        'metrics_names.h',
        'pref_names.cc',
        'pref_names.h',
        'prefs.cc',
        'prefs.h',
        'saved_files_service.cc',
        'saved_files_service.h',
        'saved_files_service_factory.cc',
        'saved_files_service_factory.h',
        'size_constraints.cc',
        'size_constraints.h',
        'switches.cc',
        'switches.h',
        'ui/native_app_window.h',
        'ui/views/app_window_frame_view.cc',
        'ui/views/app_window_frame_view.h',
        'ui/views/native_app_window_views.cc',
        'ui/views/native_app_window_views.h',
      ],
      'conditions': [
        ['chromeos==1',
          {
            'dependencies': [
              'browser_chromeos',
            ]
          }
        ],
        ['enable_extensions==0',
          {
            'sources/': [
              ['exclude', '^apps/'],
            ],
          }
        ],
        ['toolkit_views==1', {
          'dependencies': [
            '../ui/base/strings/ui_strings.gyp:ui_strings',
            '../ui/views/views.gyp:views',
          ],
        }, {  # toolkit_views==0
          'sources/': [
            ['exclude', 'ui/views/'],
          ],
        }],
      ],
      # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
      'msvs_disabled_warnings': [ 4267, ],
    },
  ],  # targets
  'conditions': [
    ['chromeos==1 or (OS=="linux" and use_aura==1) or (OS=="win" and use_aura==1)', {
      'targets': [
        {
          'target_name': 'app_shell_pak',
          'type': 'none',
          'dependencies': [
            # Need extension related resources in common_resources.pak and
            # renderer_resources_100_percent.pak
            'chrome_resources.gyp:chrome_resources',
            # Need app related resources in theme_resources_100_percent.pak
            'chrome_resources.gyp:theme_resources',
            # Need dev-tools related resources in shell_resources.pak and
            # devtools_resources.pak.
            '../content/content_shell_and_tests.gyp:generate_content_shell_resources',
            '../content/browser/devtools/devtools_resources.gyp:devtools_resources',
            '../ui/base/strings/ui_strings.gyp:ui_strings',
            '../ui/resources/ui_resources.gyp:ui_resources',
          ],
          'actions': [
            {
              'action_name': 'repack_app_shell_pack',
              'variables': {
                'pak_inputs': [
                  '<(grit_out_dir)/common_resources.pak',
                  '<(grit_out_dir)/extensions_api_resources.pak',
                  # TODO(jamescook): extra the extension/app related resources
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
             'includes': [ '../build/repack_action.gypi' ],
            },
          ],
        },
        {
          'target_name': 'app_shell_lib',
          'type': 'static_library',
          'defines!': ['CONTENT_IMPLEMENTATION'],
          'variables': {
            'chromium_code': 1,
          },
          'dependencies': [
            'app_shell_pak',
            'apps',
            'common/extensions/api/api.gyp:chrome_api',
            'test_support_common',
            '../base/base.gyp:base',
            '../base/base.gyp:base_prefs_test_support',
            '../content/content.gyp:content',
            '../content/content_shell_and_tests.gyp:content_shell_lib',
            '../extensions/common/api/api.gyp:extensions_api',
            '../skia/skia.gyp:skia',
            '../ui/views/views.gyp:views',
            '../ui/wm/wm.gyp:wm_test_support',
          ],
          'include_dirs': [
            '..',
          ],
          'sources': [
            'shell/app/shell_main_delegate.cc',
            'shell/app/shell_main_delegate.h',
            'shell/browser/shell_app_sorting.cc',
            'shell/browser/shell_app_sorting.h',
            'shell/browser/shell_app_window_delegate.cc',
            'shell/browser/shell_app_window_delegate.h',
            'shell/browser/shell_apps_client.cc',
            'shell/browser/shell_apps_client.h',
            'shell/browser/shell_browser_context.cc',
            'shell/browser/shell_browser_context.h',
            'shell/browser/shell_browser_main_parts.cc',
            'shell/browser/shell_browser_main_parts.h',
            'shell/browser/shell_content_browser_client.cc',
            'shell/browser/shell_content_browser_client.h',
            'shell/browser/shell_desktop_controller.cc',
            'shell/browser/shell_desktop_controller.h',
            'shell/browser/shell_extension_system.cc',
            'shell/browser/shell_extension_system.h',
            'shell/browser/shell_extension_system_factory.cc',
            'shell/browser/shell_extension_system_factory.h',
            'shell/browser/shell_extension_web_contents_observer.cc',
            'shell/browser/shell_extension_web_contents_observer.h',
            'shell/browser/shell_extensions_browser_client.cc',
            'shell/browser/shell_extensions_browser_client.h',
            'shell/common/shell_content_client.cc',
            'shell/common/shell_content_client.h',
            'shell/common/shell_extensions_client.cc',
            'shell/common/shell_extensions_client.h',
            'shell/renderer/shell_content_renderer_client.cc',
            'shell/renderer/shell_content_renderer_client.h',
          ],
        },
        {
          'target_name': 'app_shell',
          'type': 'executable',
          'defines!': ['CONTENT_IMPLEMENTATION'],
          'variables': {
            'chromium_code': 1,
          },
          'dependencies': [
            'app_shell_lib',
            'app_shell_pak',
          ],
          'include_dirs': [
            '..',
          ],
          'sources': [
            'shell/app/shell_main.cc',
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
                '../sandbox/sandbox.gyp:sandbox',
              ],
            }],
          ],
        },
        {
          'target_name': 'apps_browsertests',
          'type': '<(gtest_target_type)',
          'variables': {
            'chromium_code': 1,
          },
          'dependencies': [
            'app_shell_lib',
            # TODO(yoz): find the right deps
            '../base/base.gyp:test_support_base',
            '../content/content.gyp:content_app_both',
            '../content/content_shell_and_tests.gyp:content_browser_test_support',
            '../content/content_shell_and_tests.gyp:test_support_content',
            '../testing/gtest.gyp:gtest',
          ],
          'defines': [
            'HAS_OUT_OF_PROC_TEST_RUNNER',
          ],
          'msvs_large_pdb': 1,
          'sources': [
            # TODO(yoz): Refactor once we have a second test target.
            'test/app_shell_test.h',
            'test/app_shell_test.cc',
            'test/apps_test_launcher_delegate.cc',
            'test/apps_test_launcher_delegate.h',
            'test/apps_tests_main.cc',
          ],
        },
      ],  # targets
    }],  # chromeos==1 or linux aura or win aura
  ],  # conditions
}
