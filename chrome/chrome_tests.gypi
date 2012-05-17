# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'variables' : {
    'pyautolib_sources': [
      'app/chrome_command_ids.h',
      'app/chrome_dll_resource.h',
      'common/automation_constants.h',
      'common/pref_names.cc',
      'common/pref_names.h',
      'test/automation/browser_proxy.cc',
      'test/automation/browser_proxy.h',
      'test/automation/tab_proxy.cc',
      'test/automation/tab_proxy.h',
      '../content/public/common/page_type.h',
      '../content/public/common/security_style.h',
      # Must come before cert_status_flags.h
      '../net/base/net_export.h',
      '../net/base/cert_status_flags.h',
    ],
    'conditions': [
      ['asan==1', {
        'pyautolib_sources': [
          'test/pyautolib/asan_stub.c',
        ]
      }],
    ],
  },
  'includes': [
    'js_unittest_vars.gypi',
  ],
  'targets': [
    {
      # This target contains mocks and test utilities that don't belong in
      # production libraries but are used by more than one test executable.
      'target_name': 'test_support_common',
      'type': 'static_library',
      'dependencies': [
        'app/policy/cloud_policy_codegen.gyp:policy',
        'browser',
        '../sync/protocol/sync_proto.gyp:sync_proto',
        'chrome_resources.gyp:chrome_resources',
        'chrome_resources.gyp:chrome_strings',
        'chrome_resources.gyp:theme_resources',
        'common',
        'common/extensions/api/api.gyp:api',
        'plugin',
        'renderer',
        'utility',
        '../base/base.gyp:test_support_base',
        '../content/content.gyp:content_app',
        '../content/content.gyp:content_gpu',
        '../content/content.gyp:content_plugin',
        '../content/content.gyp:content_ppapi_plugin',
        '../content/content.gyp:content_renderer',
        '../content/content.gyp:content_utility',
        '../content/content.gyp:content_worker',
        '../content/content.gyp:test_support_content',
        '../ipc/ipc.gyp:test_support_ipc',
        '../media/media.gyp:media_test_support',
        '../net/net.gyp:net',
        '../net/net.gyp:net_test_support',
        '../skia/skia.gyp:skia',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        '../third_party/leveldatabase/leveldatabase.gyp:leveldatabase',
        '../ui/compositor/compositor.gyp:compositor_test_support',
      ],
      'export_dependent_settings': [
        'renderer',
        'app/policy/cloud_policy_codegen.gyp:policy',
        '../base/base.gyp:test_support_base',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'app/breakpad_mac_stubs.mm',
        'app/chrome_main_delegate.cc',
        'app/chrome_main_delegate.h',
        'browser/autofill/autofill_common_test.cc',
        'browser/autofill/autofill_common_test.h',
        'browser/autofill/data_driven_test.cc',
        'browser/autofill/data_driven_test.h',
        'browser/autofill/test_autofill_external_delegate.cc',
        'browser/autofill/test_autofill_external_delegate.h',
        'browser/automation/mock_tab_event_observer.cc',
        'browser/automation/mock_tab_event_observer.h',
        'browser/chromeos/cros/mock_cert_library.cc',
        'browser/chromeos/cros/mock_cert_library.h',
        'browser/chromeos/cros/mock_cryptohome_library.cc',
        'browser/chromeos/cros/mock_cryptohome_library.h',
        'browser/chromeos/cros/mock_library_loader.cc',
        'browser/chromeos/cros/mock_library_loader.h',
        'browser/chromeos/cros/mock_network_library.cc',
        'browser/chromeos/cros/mock_network_library.h',
        'browser/chromeos/cryptohome/mock_async_method_caller.cc',
        'browser/chromeos/cryptohome/mock_async_method_caller.h',
        'browser/chromeos/input_method/mock_candidate_window.cc',
        'browser/chromeos/input_method/mock_candidate_window.h',
        'browser/chromeos/input_method/mock_input_method_manager.cc',
        'browser/chromeos/input_method/mock_input_method_manager.h',
        'browser/chromeos/input_method/mock_xkeyboard.cc',
        'browser/chromeos/input_method/mock_xkeyboard.h',
        'browser/chromeos/login/mock_login_status_consumer.cc',
        'browser/chromeos/login/mock_login_status_consumer.h',
        'browser/chromeos/login/mock_login_utils.cc',
        'browser/chromeos/login/mock_login_utils.h',
        'browser/chromeos/login/mock_signed_settings_helper.cc',
        'browser/chromeos/login/mock_signed_settings_helper.h',
        'browser/chromeos/login/mock_url_fetchers.cc',
        'browser/chromeos/login/mock_url_fetchers.h',
        'browser/chromeos/login/mock_user_manager.cc',
        'browser/chromeos/login/mock_user_manager.h',
        # The only thing used from browser is Browser::Type.
        'browser/extensions/mock_extension_special_storage_policy.cc',
        'browser/extensions/mock_extension_special_storage_policy.h',
        'browser/extensions/test_extension_prefs.cc',
        'browser/extensions/test_extension_prefs.h',
        'browser/extensions/test_extension_service.cc',
        'browser/extensions/test_extension_service.h',
        'browser/extensions/test_extension_system.cc',
        'browser/extensions/test_extension_system.h',
        'browser/mock_browsing_data_appcache_helper.cc',
        'browser/mock_browsing_data_appcache_helper.h',
        'browser/mock_browsing_data_cookie_helper.cc',
        'browser/mock_browsing_data_cookie_helper.h',
        'browser/mock_browsing_data_database_helper.cc',
        'browser/mock_browsing_data_database_helper.h',
        'browser/mock_browsing_data_file_system_helper.cc',
        'browser/mock_browsing_data_file_system_helper.h',
        'browser/mock_browsing_data_indexed_db_helper.cc',
        'browser/mock_browsing_data_indexed_db_helper.h',
        'browser/mock_browsing_data_local_storage_helper.cc',
        'browser/mock_browsing_data_local_storage_helper.h',
        'browser/mock_browsing_data_quota_helper.cc',
        'browser/mock_browsing_data_quota_helper.h',
        'browser/mock_browsing_data_server_bound_cert_helper.cc',
        'browser/mock_browsing_data_server_bound_cert_helper.h',
        'browser/notifications/notification_test_util.cc',
        'browser/notifications/notification_test_util.h',
        'browser/password_manager/mock_password_store.cc',
        'browser/password_manager/mock_password_store.h',
        'browser/policy/mock_cloud_policy_data_store.cc',
        'browser/policy/mock_cloud_policy_data_store.h',
        'browser/prefs/pref_observer_mock.cc',
        'browser/prefs/pref_observer_mock.h',
        'browser/prefs/pref_service_mock_builder.cc',
        'browser/prefs/pref_service_mock_builder.h',
        'browser/prefs/testing_pref_store.cc',
        'browser/prefs/testing_pref_store.h',
        'browser/protector/mock_protector_service.cc',
        'browser/protector/mock_protector_service.h',
        'browser/protector/mock_setting_change.cc',
        'browser/protector/mock_setting_change.h',
        'browser/search_engines/template_url_service_test_util.cc',
        'browser/search_engines/template_url_service_test_util.h',
        'browser/sessions/session_service_test_helper.cc',
        'browser/sessions/session_service_test_helper.h',
        'browser/ssl/ssl_client_auth_requestor_mock.cc',
        'browser/ssl/ssl_client_auth_requestor_mock.h',
        'browser/sync/profile_sync_service_mock.cc',
        'browser/sync/profile_sync_service_mock.h',
        'browser/ui/browser.h',
        'browser/ui/cocoa/run_loop_testing.h',
        'browser/ui/cocoa/run_loop_testing.mm',
        'browser/ui/panels/base_panel_browser_test.cc',
        'browser/ui/panels/base_panel_browser_test.h',
        'browser/ui/panels/test_panel_mouse_watcher.cc',
        'browser/ui/panels/test_panel_mouse_watcher.h',
        'browser/ui/tab_contents/test_tab_contents_wrapper.cc',
        'browser/ui/tab_contents/test_tab_contents_wrapper.h',
        'common/net/gaia/mock_url_fetcher_factory.h',
        'common/pref_store_observer_mock.cc',
        'common/pref_store_observer_mock.h',
        'renderer/chrome_mock_render_thread.cc',
        'renderer/chrome_mock_render_thread.h',
        'renderer/mock_printer.cc',
        'renderer/mock_printer.h',
        'renderer/safe_browsing/mock_feature_extractor_clock.cc',
        'renderer/safe_browsing/mock_feature_extractor_clock.h',
        'test/automation/automation_handle_tracker.cc',
        'test/automation/automation_handle_tracker.h',
        'test/automation/automation_json_requests.cc',
        'test/automation/automation_json_requests.h',
        'test/automation/automation_proxy.cc',
        'test/automation/automation_proxy.h',
        'test/automation/browser_proxy.cc',
        'test/automation/browser_proxy.h',
        'test/automation/dom_element_proxy.cc',
        'test/automation/dom_element_proxy.h',
        'test/automation/javascript_execution_controller.cc',
        'test/automation/javascript_execution_controller.h',
        'test/automation/javascript_message_utils.h',
        'test/automation/tab_proxy.cc',
        'test/automation/tab_proxy.h',
        'test/automation/value_conversion_traits.cc',
        'test/automation/value_conversion_traits.h',
        'test/automation/value_conversion_util.h',
        'test/automation/window_proxy.cc',
        'test/automation/window_proxy.h',
        'test/base/bookmark_load_observer.cc',
        'test/base/bookmark_load_observer.h',
        'test/base/chrome_process_util.cc',
        'test/base/chrome_process_util.h',
        'test/base/chrome_process_util_mac.cc',
        'test/base/chrome_render_view_host_test_harness.cc',
        'test/base/chrome_render_view_host_test_harness.h',
        'test/base/chrome_test_suite.cc',
        'test/base/chrome_test_suite.h',
        'test/base/in_process_browser_test.cc',
        'test/base/in_process_browser_test.h',
        'test/base/model_test_utils.cc',
        'test/base/model_test_utils.h',
        'test/base/module_system_test.cc',
        'test/base/module_system_test.h',
        'test/base/profile_mock.cc',
        'test/base/profile_mock.h',
        'test/base/test_browser_window.cc',
        'test/base/test_browser_window.h',
        'test/base/test_launcher_utils.cc',
        'test/base/test_launcher_utils.h',
        'test/base/test_location_bar.cc',
        'test/base/test_location_bar.h',
        'test/base/test_switches.cc',
        'test/base/test_switches.h',
        'test/base/test_tab_strip_model_observer.cc',
        'test/base/test_tab_strip_model_observer.h',
        'test/base/test_web_dialog_observer.cc',
        'test/base/test_web_dialog_observer.h',
        'test/base/testing_browser_process.cc',
        'test/base/testing_browser_process.h',
        'test/base/testing_pref_service.cc',
        'test/base/testing_pref_service.h',
        'test/base/testing_profile.cc',
        'test/base/testing_profile.h',
        'test/base/testing_profile_manager.cc',
        'test/base/testing_profile_manager.h',
        'test/base/thread_observer_helper.h',
        'test/base/tracing.cc',
        'test/base/tracing.h',
        'test/base/ui_test_utils.cc',
        'test/base/ui_test_utils.h',
        'test/base/ui_test_utils_aura.cc',
        'test/base/ui_test_utils_gtk.cc',
        'test/base/ui_test_utils_mac.mm',
        'test/base/ui_test_utils_win.cc',
        'test/logging/win/file_logger.cc',
        'test/logging/win/file_logger.h',
        'test/logging/win/log_file_printer.cc',
        'test/logging/win/log_file_printer.h',
        'test/logging/win/log_file_reader.cc',
        'test/logging/win/log_file_reader.h',
        'test/logging/win/mof_data_parser.cc',
        'test/logging/win/mof_data_parser.h',
        'test/logging/win/test_log_collector.cc',
        'test/logging/win/test_log_collector.h',
        'test/perf/perf_test.cc',
        'test/perf/perf_test.h',
        # TODO:  these should live here but are currently used by
        # production code code in libbrowser (in chrome.gyp).
        #'../content/browser/net/url_request_mock_http_job.cc',
        #'../content/browser/net/url_request_mock_http_job.h',
        '../content/test/notification_observer_mock.cc',
        '../content/test/notification_observer_mock.h',
        '../ui/gfx/image/image_unittest_util.h',
        '../ui/gfx/image/image_unittest_util.cc',
        '../webkit/fileapi/mock_file_system_options.cc',
        '../webkit/fileapi/mock_file_system_options.h',
        '../webkit/quota/mock_quota_manager.cc',
        '../webkit/quota/mock_quota_manager.h',
      ],
      'conditions': [
        ['chromeos==0', {
          'sources/': [
            ['exclude', '^browser/chromeos'],
          ],
        }],
        ['chromeos==1', {
          'dependencies': [
            '../build/linux/system.gyp:dbus',
            '../chromeos/chromeos.gyp:chromeos_test_support',
          ],
        }],
        ['toolkit_uses_gtk == 1', {
          'dependencies': [
            '../build/linux/system.gyp:gtk',
          ],
        }],
        ['toolkit_uses_gtk == 1 or chromeos==1 or (OS=="linux" and use_aura==1)', {
          'dependencies': [
            '../build/linux/system.gyp:ssl',
          ],
        }],
        ['OS!="android"', {
          'dependencies': [
            'service',
          ],
        }],
        ['OS=="win"', {
          'include_dirs': [
            '<(DEPTH)/third_party/wtl/include',
          ],
        }],
        ['OS=="win" and use_aura==1', {
          'sources/': [
            ['exclude', 'test/base/ui_test_utils_win.cc'],
          ],
        }],
      ],
    },
    {
      'target_name': 'test_support_ui',
      'type': 'static_library',
      'dependencies': [
        'chrome_resources.gyp:chrome_resources',
        'chrome_resources.gyp:chrome_strings',
        'chrome_resources.gyp:theme_resources',
        'test_support_common',
        '../skia/skia.gyp:skia',
        '../testing/gtest.gyp:gtest',
      ],
      'export_dependent_settings': [
        'test_support_common',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'test/automation/proxy_launcher.cc',
        'test/automation/proxy_launcher.h',
        'test/base/layout_test_http_server.cc',
        'test/base/layout_test_http_server.h',
        'test/reliability/automated_ui_test_base.cc',
        'test/reliability/automated_ui_test_base.h',
        'test/ui/javascript_test_util.cc',
        'test/ui/run_all_unittests.cc',
        'test/ui/ui_perf_test.cc',
        'test/ui/ui_perf_test.h',
        'test/ui/ui_test.cc',
        'test/ui/ui_test.h',
        'test/ui/ui_test_suite.cc',
        'test/ui/ui_test_suite.h',
      ],
      'conditions': [
        ['OS=="win"', {
          'dependencies': [
            'chrome.gyp:crash_service',  # run time dependency
            'chrome.gyp:crash_service_win64',  # run time dependency
          ],
        }],
        ['toolkit_uses_gtk == 1', {
          'dependencies': [
            '../build/linux/system.gyp:gtk',
          ],
        }],
      ],
    },
    {
      'target_name': 'test_support_unit',
      'type': 'static_library',
      'dependencies': [
        'chrome_resources.gyp:chrome_resources',
        'chrome_resources.gyp:chrome_strings',
        'test_support_common',
        '../skia/skia.gyp:skia',
        '../testing/gtest.gyp:gtest',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'test/base/run_all_unittests.cc',
      ],
      'conditions': [
        ['toolkit_uses_gtk == 1', {
          'dependencies': [
            # Needed for the following #include chain:
            #   test/base/run_all_unittests.cc
            #   test/base/chrome_test_suite.h
            #   gtk/gtk.h
            '../build/linux/system.gyp:gtk',
          ],
        }],
      ],
    },
    {
      'target_name': 'automated_ui_tests',
      'type': 'executable',
      'dependencies': [
        'browser',
        'chrome_resources.gyp:theme_resources',
        'renderer',
        'test_support_common',
        'test_support_ui',
        '../base/base.gyp:base',
        '../skia/skia.gyp:skia',
        '../third_party/libxml/libxml.gyp:libxml',
        '../testing/gtest.gyp:gtest',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'test/reliability/automated_ui_test_interactive_test.cc',
        'test/reliability/automated_ui_tests.cc',
        'test/reliability/automated_ui_tests.h',
      ],
      'conditions': [
        ['OS=="win" and buildtype=="Official"', {
          'configurations': {
            'Release': {
              'msvs_settings': {
                'VCCLCompilerTool': {
                  'WholeProgramOptimization': 'false',
                },
              },
            },
          },
        },],
        ['use_x11 == 1', {
          'dependencies': [
            '../tools/xdisplaycheck/xdisplaycheck.gyp:xdisplaycheck',
          ],
        }],
        ['OS=="win"', {
          'include_dirs': [
            '<(DEPTH)/third_party/wtl/include',
          ],
          'conditions': [
            ['win_use_allocator_shim==1', {
              'dependencies': [
                '<(allocator_target)',
              ],
            }],
          ],
        }],
      ],
    },
    {
      'target_name': 'interactive_ui_tests',
      'type': 'executable',
      'dependencies': [
        '../sync/protocol/sync_proto.gyp:sync_proto',
        '../sync/sync.gyp:syncapi_core',
        'chrome',
        'chrome_resources.gyp:chrome_resources',
        'chrome_resources.gyp:chrome_strings',
        'debugger',
        'test_support_common',
        # NOTE: don't add test_support_ui, no more UITests. See
        # http://crbug.com/137365
        '../third_party/hunspell/hunspell.gyp:hunspell',
        '../net/net.gyp:net',
        '../net/net.gyp:net_resources',
        '../net/net.gyp:net_test_support',
        '../skia/skia.gyp:skia',
        '../third_party/icu/icu.gyp:icui18n',
        '../third_party/icu/icu.gyp:icuuc',
        '../third_party/libpng/libpng.gyp:libpng',
        '../third_party/zlib/zlib.gyp:zlib',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        '../third_party/npapi/npapi.gyp:npapi',
        # run time dependency
        '../webkit/support/webkit_support.gyp:webkit_resources',
      ],
      'include_dirs': [
        '..',
      ],
      'defines': [
        'HAS_OUT_OF_PROC_TEST_RUNNER',
        'BROWSER_TESTS_HEADER_OVERRIDE="chrome/test/base/in_process_browser_test.h"',
      ],
      'sources': [
        'browser/browser_focus_uitest.cc',
        'browser/browser_keyevents_browsertest.cc',
        'browser/instant/instant_browsertest.cc',
        'browser/keyboard_access_browsertest.cc',
        'browser/printing/print_dialog_cloud_interative_uitest.cc',
        'browser/task_manager/task_manager_browsertest_util.cc',
        'browser/ui/gtk/bookmarks/bookmark_bar_gtk_interactive_uitest.cc',
        'browser/ui/omnibox/omnibox_view_browsertest.cc',
        'browser/ui/panels/detached_panel_browsertest.cc',
        'browser/ui/panels/docked_panel_browsertest.cc',
        'browser/ui/panels/panel_browsertest.cc',
        'browser/ui/panels/panel_drag_browsertest.cc',
        'browser/ui/panels/panel_resize_browsertest.cc',
        'browser/ui/views/bookmarks/bookmark_bar_view_test.cc',
        'browser/ui/views/button_dropdown_test.cc',
        'browser/ui/views/find_bar_host_interactive_uitest.cc',
        'browser/ui/views/menu_item_view_test.cc',
        'browser/ui/views/menu_model_adapter_test.cc',
        'browser/ui/views/ssl_client_certificate_selector_browsertest.cc',
        'browser/ui/views/tabs/tab_drag_controller_interactive_uitest.cc',
        'browser/ui/views/tabs/tab_drag_controller_interactive_uitest.h',
        'browser/ui/views/tabs/tab_drag_controller_interactive_uitest_win.cc',
        'test/base/chrome_test_launcher.cc',
        'test/base/view_event_test_base.cc',
        'test/base/view_event_test_base.h',
        '../content/app/startup_helper_win.cc',
        '../content/browser/mouseleave_browsertest.cc',
        '../content/test/test_launcher.cc',
        '../content/test/test_launcher.h',
      ],
      'conditions': [
        ['toolkit_uses_gtk == 1', {
          'dependencies': [
            '../build/linux/system.gyp:gtk',
            '../tools/xdisplaycheck/xdisplaycheck.gyp:xdisplaycheck',
          ],
          'sources!': [
            'browser/ui/views/tabs/tab_drag_controller_interactive_uitest.cc',
          ],
        }],
        ['toolkit_uses_gtk == 1 or chromeos==1 or (OS=="linux" and use_aura==1)', {
          'dependencies': [
            '../build/linux/system.gyp:ssl',
          ],
        }],
        ['toolkit_uses_gtk == 1 and toolkit_views == 0', {
          'sources!': [
            # TODO(port)
            'browser/ui/views/crypto_module_password_dialog_view_unittest.cc',
            'browser/ui/views/bookmarks/bookmark_bar_view_test.cc',
            'browser/ui/views/button_dropdown_test.cc',
            'browser/ui/views/find_bar_host_interactive_uitest.cc',
            'browser/ui/views/menu_item_view_test.cc',
            'browser/ui/views/menu_model_adapter_test.cc',
            'test/base/view_event_test_base.cc',
            'test/base/view_event_test_base.h',
          ],
        }],
        ['OS=="linux" and toolkit_views==1', {
          'sources!': [
            # TODO(port)
            'browser/ui/gtk/bookmarks/bookmark_bar_gtk_interactive_uitest.cc',
          ],
        }],
        ['OS=="mac"', {
          'sources!': [
            # TODO(port)
            'browser/ui/views/bookmarks/bookmark_bar_view_test.cc',
            'browser/ui/views/button_dropdown_test.cc',
            'browser/ui/views/find_bar_host_interactive_uitest.cc',
            'browser/ui/views/menu_item_view_test.cc',
            'browser/ui/views/menu_model_adapter_test.cc',
            'browser/ui/views/tabs/tab_drag_controller_interactive_uitest.cc',
            'test/base/view_event_test_base.cc',
            'test/base/view_event_test_base.h',
          ],
          # See comment about the same line in chrome/chrome_tests.gypi.
          'xcode_settings': {'OTHER_LDFLAGS': ['-Wl,-ObjC']},
        }],  # OS=="mac"
        ['notifications==0', {
          'sources/': [
            ['exclude', '^browser/notifications/'],
          ],
        }],
        ['toolkit_views==1', {
          'dependencies': [
            '../ui/views/views.gyp:views',
          ],
        }],
        ['chromeos==1', {
          'sources': [
            'browser/chromeos/cros/cros_in_process_browser_test.cc',
            'browser/chromeos/cros/cros_in_process_browser_test.h',
            'browser/chromeos/cros/cros_mock.cc',
            'browser/chromeos/cros/cros_mock.h',
            'browser/chromeos/login/mock_authenticator.cc',
            'browser/chromeos/login/mock_authenticator.h',
            'browser/chromeos/login/screen_locker_tester.cc',
            'browser/chromeos/login/screen_locker_tester.h',
          ],
          'sources!': [
            'browser/ui/panels/detached_panel_browsertest.cc',
            'browser/ui/panels/docked_panel_browsertest.cc',
            'browser/ui/panels/panel_browsertest.cc',
            'browser/ui/panels/panel_drag_browsertest.cc',
            'browser/ui/panels/panel_resize_browsertest.cc',
          ],
        }],
        ['OS=="win"', {
          'include_dirs': [
            '../third_party/wtl/include',
          ],
          'dependencies': [
            'chrome.gyp:chrome_version_resources',
            'chrome.gyp:installer_util_strings',
            '../sandbox/sandbox.gyp:sandbox',
            '../third_party/iaccessible2/iaccessible2.gyp:iaccessible2',
            '../third_party/isimpledom/isimpledom.gyp:isimpledom',
            '../ui/ui.gyp:ui_resources',
            '../ui/ui.gyp:ui_resources_standard',
          ],
          'sources': [
            '../webkit/glue/resources/aliasb.cur',
            '../webkit/glue/resources/cell.cur',
            '../webkit/glue/resources/col_resize.cur',
            '../webkit/glue/resources/copy.cur',
            '../webkit/glue/resources/none.cur',
            '../webkit/glue/resources/row_resize.cur',
            '../webkit/glue/resources/vertical_text.cur',
            '../webkit/glue/resources/zoom_in.cur',
            '../webkit/glue/resources/zoom_out.cur',

            'app/chrome_dll.rc',
            'test/data/resource.rc',

            # TODO:  It would be nice to have these pulled in
            # automatically from direct_dependent_settings in
            # their various targets (net.gyp:net_resources, etc.),
            # but that causes errors in other targets when
            # resulting .res files get referenced multiple times.
            '<(SHARED_INTERMEDIATE_DIR)/chrome/browser_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/common_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/extensions_api_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/renderer_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/theme_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/theme_resources_standard.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome_version/other_version.rc',
            '<(SHARED_INTERMEDIATE_DIR)/content/content_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/net/net_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/ui/ui_resources/ui_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/ui/ui_resources_standard/ui_resources_standard.rc',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/webkit_chromium_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/webkit_resources.rc',

            'browser/accessibility/accessibility_win_browsertest.cc',
            'browser/accessibility/browser_views_accessibility_browsertest.cc',
          ],
          'conditions': [
            ['win_use_allocator_shim==1', {
              'dependencies': [
                 '../base/allocator/allocator.gyp:allocator',
              ],
            }],
            ['use_aura==1', {
              'sources/': [
                ['exclude', '^browser/accessibility/accessibility_win_browsertest.cc'],
                ['exclude', '^browser/accessibility/browser_views_accessibility_browsertest.cc'],
              ],
            }],
          ],
          'configurations': {
            'Debug_Base': {
              'msvs_settings': {
                'VCLinkerTool': {
                  'LinkIncremental': '<(msvs_debug_link_nonincremental)',
                },
              },
            },
          },  # configurations
        }, { # else: OS != "win"
          'sources!': [
            'browser/ui/views/ssl_client_certificate_selector_browsertest.cc',
          ],
        }],  # OS != "win"
      ],  # conditions
    },
    {
      # Third-party support sources for chromedriver_lib.
      'target_name': 'chromedriver_support',
      'type': 'static_library',
      'sources': [
        '../third_party/mongoose/mongoose.c',
        '../third_party/mongoose/mongoose.h',
        '../third_party/webdriver/atoms.cc',
        '../third_party/webdriver/atoms.h',
      ],
    },
    {
      # chromedriver is the chromium implementation of WebDriver.
      # See http://www.chromium.org/developers/testing/webdriver-for-chrome
      'target_name': 'chromedriver_lib',
      'type': 'static_library',
      'dependencies': [
        'browser',
        'chrome',
        'chrome_resources.gyp:chrome_resources',
        'chrome_resources.gyp:chrome_strings',
        'chromedriver_support',
        'common',
        'test_support_ui',
        '../base/base.gyp:base',
        '../build/temp_gyp/googleurl.gyp:googleurl',
        '../net/net.gyp:net',
        '../skia/skia.gyp:skia',
        '../sync/sync.gyp:syncapi_core',
        '../testing/gtest.gyp:gtest',
        '../third_party/icu/icu.gyp:icui18n',
        '../third_party/icu/icu.gyp:icuuc',
        '../third_party/libxml/libxml.gyp:libxml',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'test/webdriver/commands/alert_commands.cc',
        'test/webdriver/commands/alert_commands.h',
        'test/webdriver/commands/appcache_status_command.cc',
        'test/webdriver/commands/appcache_status_command.h',
        'test/webdriver/commands/browser_connection_commands.cc',
        'test/webdriver/commands/browser_connection_commands.h',
        'test/webdriver/commands/chrome_commands.cc',
        'test/webdriver/commands/chrome_commands.h',
        'test/webdriver/commands/command.cc',
        'test/webdriver/commands/command.h',
        'test/webdriver/commands/cookie_commands.cc',
        'test/webdriver/commands/cookie_commands.h',
        'test/webdriver/commands/create_session.cc',
        'test/webdriver/commands/create_session.h',
        'test/webdriver/commands/execute_async_script_command.cc',
        'test/webdriver/commands/execute_async_script_command.h',
        'test/webdriver/commands/execute_command.cc',
        'test/webdriver/commands/execute_command.h',
        'test/webdriver/commands/file_upload_command.cc',
        'test/webdriver/commands/file_upload_command.h',
        'test/webdriver/commands/find_element_commands.cc',
        'test/webdriver/commands/find_element_commands.h',
        'test/webdriver/commands/html5_location_commands.cc',
        'test/webdriver/commands/html5_location_commands.h',
        'test/webdriver/commands/html5_storage_commands.cc',
        'test/webdriver/commands/html5_storage_commands.h',
        'test/webdriver/commands/keys_command.cc',
        'test/webdriver/commands/keys_command.h',
        'test/webdriver/commands/log_command.cc',
        'test/webdriver/commands/log_command.h',
        'test/webdriver/commands/navigate_commands.cc',
        'test/webdriver/commands/navigate_commands.h',
        'test/webdriver/commands/mouse_commands.cc',
        'test/webdriver/commands/mouse_commands.h',
        'test/webdriver/commands/response.h',
        'test/webdriver/commands/response.cc',
        'test/webdriver/commands/screenshot_command.cc',
        'test/webdriver/commands/screenshot_command.h',
        'test/webdriver/commands/session_with_id.cc',
        'test/webdriver/commands/session_with_id.h',
        'test/webdriver/commands/set_timeout_commands.cc',
        'test/webdriver/commands/set_timeout_commands.h',
        'test/webdriver/commands/source_command.cc',
        'test/webdriver/commands/source_command.h',
        'test/webdriver/commands/target_locator_commands.cc',
        'test/webdriver/commands/target_locator_commands.h',
        'test/webdriver/commands/title_command.cc',
        'test/webdriver/commands/title_command.h',
        'test/webdriver/commands/url_command.cc',
        'test/webdriver/commands/url_command.h',
        'test/webdriver/commands/webdriver_command.cc',
        'test/webdriver/commands/webdriver_command.h',
        'test/webdriver/commands/webelement_commands.cc',
        'test/webdriver/commands/webelement_commands.h',
        'test/webdriver/commands/window_commands.cc',
        'test/webdriver/commands/window_commands.h',
        'test/webdriver/frame_path.cc',
        'test/webdriver/frame_path.h',
        'test/webdriver/http_response.cc',
        'test/webdriver/http_response.h',
        'test/webdriver/keycode_text_conversion.h',
        'test/webdriver/keycode_text_conversion_gtk.cc',
        'test/webdriver/keycode_text_conversion_mac.mm',
        'test/webdriver/keycode_text_conversion_win.cc',
        'test/webdriver/keycode_text_conversion_x.cc',
        'test/webdriver/webdriver_automation.cc',
        'test/webdriver/webdriver_automation.h',
        'test/webdriver/webdriver_basic_types.cc',
        'test/webdriver/webdriver_basic_types.h',
        'test/webdriver/webdriver_capabilities_parser.cc',
        'test/webdriver/webdriver_capabilities_parser.h',
        'test/webdriver/webdriver_dispatch.cc',
        'test/webdriver/webdriver_dispatch.h',
        'test/webdriver/webdriver_element_id.cc',
        'test/webdriver/webdriver_element_id.h',
        'test/webdriver/webdriver_error.cc',
        'test/webdriver/webdriver_error.h',
        'test/webdriver/webdriver_key_converter.cc',
        'test/webdriver/webdriver_key_converter.h',
        'test/webdriver/webdriver_logging.cc',
        'test/webdriver/webdriver_logging.h',
        'test/webdriver/webdriver_session.cc',
        'test/webdriver/webdriver_session.h',
        'test/webdriver/webdriver_session_manager.cc',
        'test/webdriver/webdriver_session_manager.h',
        'test/webdriver/webdriver_util.cc',
        'test/webdriver/webdriver_util.h',
        'test/webdriver/webdriver_util_mac.mm',
      ],
      'conditions': [
        ['toolkit_uses_gtk == 1', {
          'dependencies': [
            '../build/linux/system.gyp:gtk',
            '../tools/xdisplaycheck/xdisplaycheck.gyp:xdisplaycheck',
          ],
          'sources!': [
            'test/webdriver/keycode_text_conversion_x.cc',
          ],
        }],
        ['toolkit_uses_gtk == 0', {
          'sources!': [
            'test/webdriver/keycode_text_conversion_gtk.cc',
          ],
        }],
        ['OS=="linux" and toolkit_views==1', {
          'dependencies': [
            '../ui/views/views.gyp:views',
          ],
        }],
        ['os_posix == 1 and OS != "mac"', {
          'conditions': [
            ['linux_use_tcmalloc==1', {
              'dependencies': [
                '../base/allocator/allocator.gyp:allocator',
              ],
            }],
          ],
        }],
      ],
    },
    {
      'target_name': 'chromedriver',
      'type': 'executable',
      'dependencies': [
        'chromedriver_lib',
        '../base/base.gyp:base',
        '../skia/skia.gyp:skia',
        '../testing/gtest.gyp:gtest',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'test/webdriver/webdriver_server.cc',
      ],
      'conditions': [
        ['OS=="win"', {
          'conditions': [
            ['win_use_allocator_shim==1', {
              'dependencies': [
                '<(allocator_target)',
              ],
            }],
          ],
          'link_settings': {
            'libraries': [
              '-lOleAcc.lib',
              '-lws2_32.lib',
            ],
          },
          'configurations': {
            'Debug': {
              'msvs_settings': {
                'VCLinkerTool': {
                  'LinkIncremental': '<(msvs_large_module_debug_link_mode)',
                },
              },
            },
          },
        }],
      ]
    },
    {
      'target_name': 'chromedriver_unittests',
      'type': 'executable',
      'dependencies': [
        'chromedriver_lib',
        '../base/base.gyp:run_all_unittests',
        '../base/base.gyp:test_support_base',
        '../testing/gtest.gyp:gtest',
        '../skia/skia.gyp:skia',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'test/webdriver/commands/set_timeout_commands_unittest.cc',
        'test/webdriver/frame_path_unittest.cc',
        'test/webdriver/http_response_unittest.cc',
        'test/webdriver/keycode_text_conversion_unittest.cc',
        'test/webdriver/webdriver_capabilities_parser_unittest.cc',
        'test/webdriver/webdriver_dispatch_unittest.cc',
        'test/webdriver/webdriver_key_converter_unittest.cc',
        'test/webdriver/webdriver_test_util.cc',
        'test/webdriver/webdriver_test_util.h',
        'test/webdriver/webdriver_util_unittest.cc',
      ],
      'conditions': [
        ['OS=="win"', {
          'conditions': [
            ['win_use_allocator_shim==1', {
              'dependencies': [
                '<(allocator_target)',
              ],
            }],
          ],
          'link_settings': {
            'libraries': [
              '-lOleAcc.lib',
              '-lws2_32.lib',
            ],
          },
          'configurations': {
            'Debug': {
              'msvs_settings': {
                'VCLinkerTool': {
                  'LinkIncremental': '<(msvs_large_module_debug_link_mode)',
                },
              },
            },
          },
        }],
      ],
    },
    {
      'target_name': 'unit_tests',
      'type': 'executable',
      'dependencies': [
        # unit tests should only depend on
        # 1) everything that the chrome binaries depend on:
        '<@(chromium_dependencies)',
        # 2) test-specific support libraries:
        '../base/base.gyp:test_support_base',
        '../gpu/gpu.gyp:gpu_unittest_utils',
        '../media/media.gyp:media_test_support',
        '../net/net.gyp:net',
        '../net/net.gyp:net_test_support',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        'test_support_common',
        '../sync/sync.gyp:test_support_sync',
        '../sync/sync.gyp:test_support_sync_notifier',
        '../sync/sync.gyp:test_support_syncapi_core',
        '../sync/sync.gyp:test_support_syncapi_service',
        'test_support_unit',
        # 3) anything tests directly depend on
        '../skia/skia.gyp:skia',
        '../third_party/bzip2/bzip2.gyp:bzip2',
        '../third_party/cld/cld.gyp:cld',
        '../third_party/icu/icu.gyp:icui18n',
        '../third_party/icu/icu.gyp:icuuc',
        '../third_party/leveldatabase/leveldatabase.gyp:leveldatabase',
        '../third_party/libjingle/libjingle.gyp:libjingle',
        '../third_party/libxml/libxml.gyp:libxml',
        '../tools/json_schema_compiler/test/json_schema_compiler_tests.gyp:json_schema_compiler_tests',
        '../ui/gl/gl.gyp:gl',
        '../ui/ui.gyp:ui_resources',
        '../ui/ui.gyp:ui_resources_standard',
        '../ui/ui.gyp:ui_test_support',
        '../v8/tools/gyp/v8.gyp:v8',
        'common/extensions/api/api.gyp:api',
        'chrome_resources.gyp:chrome_resources',
        'chrome_resources.gyp:chrome_strings',
      ],
      'include_dirs': [
        '..',
      ],
      # TODO(scr): Use this in browser_tests too.
      'includes': [
        'js_unittest_rules.gypi',
      ],
      'defines': [
        'CLD_WINDOWS',
      ],
      'direct_dependent_settings': {
        'defines': [
          'CLD_WINDOWS',
        ],
      },
      'sources': [
        'app/breakpad_mac_stubs.mm',
        'app/chrome_dll.rc',
        # All unittests in browser, common, renderer and service.
        'browser/about_flags_unittest.cc',
        'browser/app_controller_mac_unittest.mm',
        'browser/autocomplete/autocomplete_edit_unittest.cc',
        'browser/autocomplete/autocomplete_result_unittest.cc',
        'browser/autocomplete/autocomplete_unittest.cc',
        'browser/autocomplete/builtin_provider_unittest.cc',
        'browser/autocomplete/extension_app_provider_unittest.cc',
        'browser/autocomplete/history_contents_provider_unittest.cc',
        'browser/autocomplete/history_quick_provider_unittest.cc',
        'browser/autocomplete/history_url_provider_unittest.cc',
        'browser/autocomplete/keyword_provider_unittest.cc',
        'browser/autocomplete/search_provider_unittest.cc',
        'browser/autocomplete/shortcuts_provider_unittest.cc',
        'browser/autocomplete_history_manager_unittest.cc',
        'browser/autofill/address_field_unittest.cc',
        'browser/autofill/address_unittest.cc',
        'browser/autofill/autofill_country_unittest.cc',
        'browser/autofill/autofill_download_unittest.cc',
        'browser/autofill/autofill_external_delegate_unittest.cc',
        'browser/autofill/autofill_field_unittest.cc',
        'browser/autofill/autofill_ie_toolbar_import_win_unittest.cc',
        'browser/autofill/autofill_manager_unittest.cc',
        'browser/autofill/autofill_merge_unittest.cc',
        'browser/autofill/autofill_metrics_unittest.cc',
        'browser/autofill/autofill_profile_unittest.cc',
        'browser/autofill/autofill_popup_unittest.cc',
        'browser/autofill/autofill_type_unittest.cc',
        'browser/autofill/autofill_xml_parser_unittest.cc',
        'browser/autofill/contact_info_unittest.cc',
        'browser/autofill/credit_card_field_unittest.cc',
        'browser/autofill/credit_card_unittest.cc',
        'browser/autofill/form_field_unittest.cc',
        'browser/autofill/form_structure_unittest.cc',
        'browser/autofill/name_field_unittest.cc',
        'browser/autofill/password_autofill_manager_unittest.cc',
        'browser/autofill/password_generator_unittest.cc',
        'browser/autofill/personal_data_manager_unittest.cc',
        'browser/autofill/phone_field_unittest.cc',
        'browser/autofill/phone_number_unittest.cc',
        'browser/autofill/phone_number_i18n_unittest.cc',
        'browser/autofill/select_control_handler_unittest.cc',
        'browser/automation/automation_provider_unittest.cc',
        'browser/automation/automation_tab_helper_unittest.cc',
        'browser/background/background_application_list_model_unittest.cc',
        'browser/background/background_contents_service_unittest.cc',
        'browser/background/background_mode_manager_unittest.cc',
        'browser/bookmarks/bookmark_codec_unittest.cc',
        'browser/bookmarks/bookmark_context_menu_controller_unittest.cc',
        'browser/bookmarks/bookmark_expanded_state_tracker_unittest.cc',
        'browser/bookmarks/bookmark_extension_helpers_unittest.cc',
        'browser/bookmarks/bookmark_html_writer_unittest.cc',
        'browser/bookmarks/bookmark_index_unittest.cc',
        'browser/bookmarks/bookmark_model_test_utils.cc',
        'browser/bookmarks/bookmark_model_test_utils.h',
        'browser/bookmarks/bookmark_model_unittest.cc',
        'browser/bookmarks/bookmark_node_data_unittest.cc',
        'browser/bookmarks/bookmark_utils_unittest.cc',
        'browser/bookmarks/recently_used_folders_combo_model_unittest.cc',
        'browser/browser_about_handler_unittest.cc',
        'browser/browser_commands_unittest.cc',
        'browser/browsing_data_appcache_helper_unittest.cc',
        'browser/browsing_data_cookie_helper_unittest.cc',
        'browser/browsing_data_database_helper_unittest.cc',
        'browser/browsing_data_file_system_helper_unittest.cc',
        'browser/browsing_data_helper_unittest.cc',
        'browser/browsing_data_indexed_db_helper_unittest.cc',
        'browser/browsing_data_local_storage_helper_unittest.cc',
        'browser/browsing_data_quota_helper_unittest.cc',
        'browser/browsing_data_remover_unittest.cc',
        'browser/browsing_data_server_bound_cert_helper_unittest.cc',
        'browser/captive_portal/captive_portal_service_unittest.cc',
        'browser/chrome_browser_application_mac_unittest.mm',
        'browser/chrome_browser_main_unittest.cc',
        'browser/chrome_page_zoom_unittest.cc',
        'browser/chromeos/bluetooth/bluetooth_service_record_unittest.cc',
        'browser/chromeos/bluetooth/bluetooth_utils_unittest.cc',
        'browser/chromeos/cros/cros_network_functions_unittest.cc',
        'browser/chromeos/cros/mock_chromeos_network.cc',
        'browser/chromeos/cros/mock_chromeos_network.h',
        'browser/chromeos/cros/network_constants.h',
        'browser/chromeos/cros/network_library.cc',
        'browser/chromeos/cros/network_library.h',
        'browser/chromeos/cros/network_library_unittest.cc',
        'browser/chromeos/cros/network_ui_data_unittest.cc',
        'browser/chromeos/cros/onc_network_parser_unittest.cc',
        'browser/chromeos/cros_settings_unittest.cc',
        'browser/chromeos/customization_document_unittest.cc',
        'browser/chromeos/dbus/cros_dbus_service_unittest.cc',
        'browser/chromeos/dbus/introspect_util_unittest.cc',
        'browser/chromeos/dbus/proxy_resolution_service_provider_unittest.cc',
        'browser/chromeos/extensions/file_browser_notifications_unittest.cc',
        'browser/chromeos/external_metrics_unittest.cc',
        'browser/chromeos/gdata/drive_webapps_registry_unittest.cc',
        'browser/chromeos/gdata/gdata_db_unittest.cc',
        'browser/chromeos/gdata/gdata_file_system_unittest.cc',
        'browser/chromeos/gdata/gdata_files_unittest.cc',
        'browser/chromeos/gdata/gdata_operation_registry_unittest.cc',
        'browser/chromeos/gdata/gdata_parser_unittest.cc',
        'browser/chromeos/gdata/gdata_sync_client_unittest.cc',
        'browser/chromeos/gdata/gdata_util_unittest.cc',
        'browser/chromeos/gdata/mock_directory_change_observer.cc',
        'browser/chromeos/gdata/mock_directory_change_observer.h',
        'browser/chromeos/gdata/mock_gdata_documents_service.cc',
        'browser/chromeos/gdata/mock_gdata_documents_service.h',
        'browser/chromeos/gdata/mock_gdata_file_system.cc',
        'browser/chromeos/gdata/mock_gdata_file_system.h',
        'browser/chromeos/gdata/mock_gdata_sync_client.cc',
        'browser/chromeos/gdata/mock_gdata_sync_client.h',
        'browser/chromeos/gview_request_interceptor_unittest.cc',
        'browser/chromeos/imageburner/burn_manager_unittest.cc',
        'browser/chromeos/input_method/browser_state_monitor_unittest.cc',
        'browser/chromeos/input_method/candidate_window_view_unittest.cc',
        'browser/chromeos/input_method/ibus_controller_base_unittest.cc',
        'browser/chromeos/input_method/ibus_controller_impl_unittest.cc',
        'browser/chromeos/input_method/ibus_controller_unittest.cc',
        'browser/chromeos/input_method/ibus_ui_controller_unittest.cc',
        'browser/chromeos/input_method/input_method_descriptor_unittest.cc',
        'browser/chromeos/input_method/input_method_manager_impl_unittest.cc',
        'browser/chromeos/input_method/input_method_manager_unittest.cc',
        'browser/chromeos/input_method/input_method_property_unittest.cc',
        'browser/chromeos/input_method/input_method_util_unittest.cc',
        'browser/chromeos/input_method/input_method_whitelist_unittest.cc',
        'browser/chromeos/input_method/virtual_keyboard_selector_unittest.cc',
        'browser/chromeos/input_method/xkeyboard_unittest.cc',
        'browser/chromeos/kiosk_mode/kiosk_mode_idle_logout_unittest.cc',
        'browser/chromeos/kiosk_mode/kiosk_mode_screensaver_unittest.cc',
        'browser/chromeos/kiosk_mode/kiosk_mode_settings_unittest.cc',
        'browser/chromeos/language_preferences_unittest.cc',
        'browser/chromeos/login/authenticator_unittest.cc',
        'browser/chromeos/login/mock_auth_attempt_state_resolver.cc',
        'browser/chromeos/login/mock_auth_attempt_state_resolver.h',
        'browser/chromeos/login/mock_owner_key_utils.cc',
        'browser/chromeos/login/mock_owner_key_utils.h',
        'browser/chromeos/login/mock_ownership_service.cc',
        'browser/chromeos/login/mock_ownership_service.h',
        'browser/chromeos/login/online_attempt_unittest.cc',
        'browser/chromeos/login/owner_key_utils_unittest.cc',
        'browser/chromeos/login/owner_manager_unittest.cc',
        'browser/chromeos/login/ownership_service_unittest.cc',
        'browser/chromeos/login/parallel_authenticator_unittest.cc',
        'browser/chromeos/login/signed_settings_helper_unittest.cc',
        'browser/chromeos/login/signed_settings_cache_unittest.cc',
        'browser/chromeos/login/signed_settings_unittest.cc',
        'browser/chromeos/login/user_manager_unittest.cc',
        'browser/chromeos/mobile_config_unittest.cc',
        'browser/chromeos/network_message_observer_unittest.cc',
        'browser/chromeos/offline/offline_load_page_unittest.cc',
        'browser/chromeos/oom_priority_manager_unittest.cc',
        'browser/chromeos/preferences_unittest.cc',
        'browser/chromeos/process_proxy/process_output_watcher_unittest.cc',
        'browser/chromeos/proxy_config_service_impl_unittest.cc',
        'browser/chromeos/status/network_menu_icon.cc',
        'browser/chromeos/status/network_menu_icon_unittest.cc',
        'browser/chromeos/stub_cros_settings_provider_unittest.cc',
        'browser/chromeos/system/mock_statistics_provider.cc',
        'browser/chromeos/system/mock_statistics_provider.h',
        'browser/chromeos/system/name_value_pairs_parser_unittest.cc',
        'browser/chromeos/version_loader_unittest.cc',
        'browser/chromeos/web_socket_proxy_helper_unittest.cc',
        'browser/command_updater_unittest.cc',
        'browser/component_updater/component_installers_unittest.cc',
        'browser/component_updater/component_updater_service_unittest.cc',
        'browser/component_updater/component_updater_interceptor.cc',
        'browser/component_updater/component_updater_interceptor.h',
        'browser/content_settings/content_settings_default_provider_unittest.cc',
        'browser/content_settings/content_settings_mock_observer.cc',
        'browser/content_settings/content_settings_mock_observer.h',
        'browser/content_settings/content_settings_mock_provider.cc',
        'browser/content_settings/content_settings_mock_provider.h',
        'browser/content_settings/content_settings_origin_identifier_value_map_unittest.cc',
        'browser/content_settings/content_settings_policy_provider_unittest.cc',
        'browser/content_settings/content_settings_pref_provider_unittest.cc',
        'browser/content_settings/content_settings_provider_unittest.cc',
        'browser/content_settings/content_settings_rule_unittest.cc',
        'browser/content_settings/content_settings_utils_unittest.cc',
        'browser/content_settings/cookie_settings_unittest.cc',
        'browser/content_settings/host_content_settings_map_unittest.cc',
        'browser/content_settings/mock_settings_observer.cc',
        'browser/content_settings/mock_settings_observer.h',
        'browser/content_settings/tab_specific_content_settings_unittest.cc',
        'browser/cookies_tree_model_unittest.cc',
        'browser/custom_handlers/protocol_handler_registry_unittest.cc',
        'browser/diagnostics/diagnostics_model_unittest.cc',
        'browser/download/download_item_model_unittest.cc',
        'browser/download/download_request_infobar_delegate_unittest.cc',
        'browser/download/download_request_limiter_unittest.cc',
        'browser/download/download_shelf_unittest.cc',
        'browser/download/download_status_updater_unittest.cc',
        'browser/enumerate_modules_model_unittest_win.cc',
        'browser/event_disposition.cc',
        'browser/event_disposition.h',
        'browser/extensions/api/alarms/alarms_api_unittest.cc',
        'browser/extensions/api/content_settings/content_settings_store_unittest.cc',
        'browser/extensions/api/content_settings/content_settings_unittest.cc',
        'browser/extensions/api/cookies/cookies_unittest.cc',
        'browser/extensions/api/declarative/initializing_rules_registry_unittest.cc',
        'browser/extensions/api/declarative/rules_registry_service_unittest.cc',
        'browser/extensions/api/declarative/rules_registry_with_cache_unittest.cc',
        'browser/extensions/api/declarative_webrequest/webrequest_action_unittest.cc',
        'browser/extensions/api/declarative_webrequest/webrequest_condition_attribute_unittest.cc',
        'browser/extensions/api/declarative_webrequest/webrequest_condition_unittest.cc',
        'browser/extensions/api/declarative_webrequest/webrequest_rules_registry_unittest.cc',
        'browser/extensions/api/extension_action/extension_browser_actions_api_unittest.cc',
        'browser/extensions/api/omnibox/omnibox_unittest.cc',
        'browser/extensions/api/permissions/permissions_api_helpers_unittest.cc',
        'browser/extensions/api/proxy/proxy_api_helpers_unittest.cc',
        'browser/extensions/api/serial/serial_connection_unittest.cc',
        'browser/extensions/api/socket/tcp_socket_unittest.cc',
        'browser/extensions/api/web_navigation/web_navigation_unittest.cc',
        'browser/extensions/api/web_request/web_request_api_unittest.cc',
        'browser/extensions/api/web_request/web_request_time_tracker_unittest.cc',
        'browser/extensions/app_notification_manager_sync_unittest.cc',
        'browser/extensions/app_notification_manager_unittest.cc',
        'browser/extensions/app_notification_storage_unittest.cc',
        'browser/extensions/app_notification_test_util.cc',
        'browser/extensions/app_notify_channel_setup_unittest.cc',
        'browser/extensions/app_sync_data_unittest.cc',
        'browser/extensions/apps_promo_unittest.cc',
        'browser/extensions/component_loader_unittest.cc',
        'browser/extensions/convert_user_script_unittest.cc',
        'browser/extensions/convert_web_app_unittest.cc',
        'browser/extensions/extension_creator_filter_unittest.cc',
        'browser/extensions/extension_event_router_forwarder_unittest.cc',
        'browser/extensions/extension_function_test_utils.cc',
        'browser/extensions/extension_function_test_utils.h',
        'browser/extensions/extension_icon_manager_unittest.cc',
        'browser/extensions/extension_idle_api_unittest.cc',
        'browser/extensions/extension_info_map_unittest.cc',
        'browser/extensions/extension_menu_manager_unittest.cc',
        'browser/extensions/extension_pref_value_map_unittest.cc',
        'browser/extensions/extension_prefs_unittest.cc',
        'browser/extensions/extension_prefs_unittest.h',
        'browser/extensions/extension_process_manager_unittest.cc',
        'browser/extensions/extension_protocols_unittest.cc',
        'browser/extensions/extension_service_unittest.cc',
        'browser/extensions/extension_service_unittest.h',
        'browser/extensions/extension_sorting_unittest.cc',
        'browser/extensions/extension_special_storage_policy_unittest.cc',
        'browser/extensions/extension_sync_data_unittest.cc',
        'browser/extensions/extension_ui_unittest.cc',
        'browser/extensions/extension_warning_set_unittest.cc',
        'browser/extensions/extensions_quota_service_unittest.cc',
        'browser/extensions/external_policy_extension_loader_unittest.cc',
        'browser/extensions/permissions_updater_unittest.cc',
        'browser/extensions/file_reader_unittest.cc',
        'browser/extensions/image_loading_tracker_unittest.cc',
        'browser/extensions/key_identifier_conversion_views_unittest.cc',
        'browser/extensions/process_map_unittest.cc',
        'browser/extensions/sandboxed_extension_unpacker_unittest.cc',
        'browser/extensions/settings/settings_frontend_unittest.cc',
        'browser/extensions/settings/settings_leveldb_storage_unittest.cc',
        'browser/extensions/settings/settings_quota_unittest.cc',
        'browser/extensions/settings/settings_storage_unittest.cc',
        'browser/extensions/settings/settings_storage_unittest.h',
        'browser/extensions/settings/settings_sync_unittest.cc',
        'browser/extensions/settings/settings_test_util.cc',
        'browser/extensions/settings/settings_test_util.h',
        'browser/extensions/settings/testing_settings_storage_unittest.cc',
        'browser/extensions/updater/extension_updater_unittest.cc',
        'browser/extensions/user_script_listener_unittest.cc',
        'browser/extensions/user_script_master_unittest.cc',
        'browser/extensions/webstore_inline_installer_unittest.cc',
        'browser/external_protocol/external_protocol_handler_unittest.cc',
        'browser/favicon/favicon_handler_unittest.cc',
        'browser/first_run/first_run_unittest.cc',
        'browser/geolocation/chrome_geolocation_permission_context_unittest.cc',
        'browser/geolocation/geolocation_settings_state_unittest.cc',
        'browser/geolocation/wifi_data_provider_unittest_chromeos.cc',
        'browser/global_keyboard_shortcuts_mac_unittest.mm',
        'browser/google/google_search_counter_unittest.cc',
        'browser/google/google_update_settings_unittest.cc',
        'browser/google/google_url_tracker_unittest.cc',
        'browser/google/google_util_unittest.cc',
        'browser/gpu_blacklist_unittest.cc',
        'browser/gpu_util_unittest.cc',
        'browser/history/android/android_cache_database_unittest.cc',
        'browser/history/android/android_history_provider_service_unittest.cc',
        'browser/history/android/android_history_types_unittest.cc',
        'browser/history/android/android_provider_backend_unittest.cc',
        'browser/history/android/android_urls_database_unittest.cc',
        'browser/history/android/bookmark_model_sql_handler_unittest.cc',
        'browser/history/android/urls_sql_handler_unittest.cc',
        'browser/history/android/visit_sql_handler_unittest.cc',
        'browser/history/expire_history_backend_unittest.cc',
        'browser/history/history_backend_unittest.cc',
        'browser/history/history_querying_unittest.cc',
        'browser/history/history_types_unittest.cc',
        'browser/history/history_unittest.cc',
        'browser/history/history_unittest_base.cc',
        'browser/history/history_unittest_base.h',
        'browser/history/in_memory_url_index_types_unittest.cc',
        'browser/history/in_memory_url_index_unittest.cc',
        'browser/history/query_parser_unittest.cc',
        'browser/history/shortcuts_backend_unittest.cc',
        'browser/history/shortcuts_database_unittest.cc',
        'browser/history/snippet_unittest.cc',
        'browser/history/starred_url_database_unittest.cc',
        'browser/history/text_database_manager_unittest.cc',
        'browser/history/text_database_unittest.cc',
        'browser/history/thumbnail_database_unittest.cc',
        'browser/history/top_sites_database_unittest.cc',
        'browser/history/top_sites_unittest.cc',
        'browser/history/url_database_unittest.cc',
        'browser/history/visit_database_unittest.cc',
        'browser/history/visit_filter_unittest.cc',
        'browser/history/visit_tracker_unittest.cc',
        'browser/importer/firefox_importer_unittest.cc',
        'browser/importer/firefox_importer_unittest_messages_internal.h',
        'browser/importer/firefox_importer_unittest_utils.h',
        'browser/importer/firefox_importer_unittest_utils_mac.cc',
        'browser/importer/firefox_importer_utils_unittest.cc',
        'browser/importer/firefox_profile_lock_unittest.cc',
        'browser/importer/firefox_proxy_settings_unittest.cc',
        'browser/importer/ie_importer_unittest_win.cc',
        'browser/importer/importer_unittest_utils.cc',
        'browser/importer/importer_unittest_utils.h',
        'browser/importer/safari_importer_unittest.mm',
        'browser/importer/toolbar_importer_unittest.cc',
        'browser/intents/cws_intents_registry_unittest.cc',
        'browser/intents/register_intent_handler_infobar_delegate_unittest.cc',
        'browser/intents/web_intents_registry_unittest.cc',
        'browser/internal_auth_unittest.cc',
        'browser/language_usage_metrics_unittest.cc',
        'browser/mac/keystone_glue_unittest.mm',
        'browser/managed_mode_unittest.cc',
        'browser/media/media_internals_unittest.cc',
        'browser/media_gallery/media_device_notifications_linux_unittest.cc',
        'browser/media_gallery/media_device_notifications_window_win_unittest.cc',
        'browser/media_gallery/media_gallery_database_unittest.cc',
        'browser/metrics/metrics_log_unittest.cc',
        'browser/metrics/metrics_log_serializer_unittest.cc',
        'browser/metrics/metrics_response_unittest.cc',
        'browser/metrics/metrics_service_unittest.cc',
        'browser/metrics/thread_watcher_unittest.cc',
        'browser/net/chrome_fraudulent_certificate_reporter_unittest.cc',
        'browser/net/chrome_net_log_unittest.cc',
        'browser/net/connection_tester_unittest.cc',
        'browser/net/gaia/gaia_oauth_fetcher_unittest.cc',
        'browser/net/http_pipelining_compatibility_client_unittest.cc',
        'browser/net/http_server_properties_manager_unittest.cc',
        'browser/net/load_timing_observer_unittest.cc',
        'browser/net/network_stats_unittest.cc',
        'browser/net/predictor_unittest.cc',
        'browser/net/pref_proxy_config_tracker_impl_unittest.cc',
        'browser/net/quoted_printable_unittest.cc',
        'browser/net/sqlite_persistent_cookie_store_unittest.cc',
        'browser/net/sqlite_server_bound_cert_store_unittest.cc',
        'browser/net/ssl_config_service_manager_pref_unittest.cc',
        'browser/net/transport_security_persister_unittest.cc',
        'browser/net/url_fixer_upper_unittest.cc',
        'browser/net/url_info_unittest.cc',
        'browser/notifications/desktop_notification_service_unittest.cc',
        'browser/parsers/metadata_parser_filebase_unittest.cc',
        'browser/password_manager/encryptor_password_mac_unittest.cc',
        'browser/password_manager/encryptor_unittest.cc',
        'browser/password_manager/login_database_unittest.cc',
        'browser/password_manager/native_backend_gnome_x_unittest.cc',
        'browser/password_manager/native_backend_kwallet_x_unittest.cc',
        'browser/password_manager/password_form_data.cc',
        'browser/password_manager/password_form_manager_unittest.cc',
        'browser/password_manager/password_manager_unittest.cc',
        'browser/password_manager/password_store_unittest.cc',
        'browser/password_manager/password_store_default_unittest.cc',
        'browser/password_manager/password_store_mac_unittest.cc',
        'browser/password_manager/password_store_win_unittest.cc',
        'browser/password_manager/password_store_x_unittest.cc',
        'browser/plugin_finder_unittest.cc',
        'browser/plugin_prefs_unittest.cc',
        'browser/policy/asynchronous_policy_loader_unittest.cc',
        'browser/policy/asynchronous_policy_provider_unittest.cc',
        'browser/policy/asynchronous_policy_test_base.cc',
        'browser/policy/asynchronous_policy_test_base.h',
        'browser/policy/auto_enrollment_client_unittest.cc',
        'browser/policy/cloud_policy_controller_unittest.cc',
        'browser/policy/cloud_policy_provider_unittest.cc',
        'browser/policy/cloud_policy_subsystem_unittest.cc',
        'browser/policy/config_dir_policy_provider_unittest.cc',
        'browser/policy/configuration_policy_handler_chromeos_unittest.cc',
        'browser/policy/configuration_policy_handler_unittest.cc',
        'browser/policy/configuration_policy_pref_store_unittest.cc',
        'browser/policy/configuration_policy_provider_mac_unittest.cc',
        'browser/policy/configuration_policy_provider_test.cc',
        'browser/policy/configuration_policy_provider_test.h',
        'browser/policy/configuration_policy_provider_win_unittest.cc',
        'browser/policy/cros_user_policy_cache_unittest.cc',
        'browser/policy/device_management_service_unittest.cc',
        'browser/policy/device_policy_cache_unittest.cc',
        'browser/policy/device_status_collector_unittest.cc',
        'browser/policy/device_token_fetcher_unittest.cc',
        'browser/policy/enterprise_install_attributes_unittest.cc',
        'browser/policy/file_based_policy_provider_unittest.cc',
        'browser/policy/logging_work_scheduler.cc',
        'browser/policy/logging_work_scheduler.h',
        'browser/policy/logging_work_scheduler_unittest.cc',
        'browser/policy/mock_configuration_policy_provider.cc',
        'browser/policy/mock_configuration_policy_provider.h',
        'browser/policy/mock_device_management_service.cc',
        'browser/policy/mock_device_management_service.h',
        'browser/policy/network_configuration_updater_unittest.cc',
        'browser/policy/policy_bundle_unittest.cc',
        'browser/policy/policy_map_unittest.cc',
        'browser/policy/policy_path_parser_unittest.cc',
        'browser/policy/policy_service_impl_unittest.cc',
        'browser/policy/testing_cloud_policy_subsystem.cc',
        'browser/policy/testing_cloud_policy_subsystem.h',
        'browser/policy/testing_policy_url_fetcher_factory.cc',
        'browser/policy/testing_policy_url_fetcher_factory.h',
        'browser/policy/url_blacklist_manager_unittest.cc',
        'browser/policy/user_policy_cache_unittest.cc',
        'browser/predictors/autocomplete_action_predictor_table_unittest.cc',
        'browser/predictors/autocomplete_action_predictor_unittest.cc',
        'browser/preferences_mock_mac.cc',
        'browser/preferences_mock_mac.h',
        'browser/prefs/command_line_pref_store_unittest.cc',
        'browser/prefs/incognito_mode_prefs_unittest.cc',
        'browser/prefs/overlay_user_pref_store_unittest.cc',
        'browser/prefs/pref_change_registrar_unittest.cc',
        'browser/prefs/pref_member_unittest.cc',
        'browser/prefs/pref_model_associator_unittest.cc',
        'browser/prefs/pref_notifier_impl_unittest.cc',
        'browser/prefs/pref_service_unittest.cc',
        'browser/prefs/pref_set_observer_unittest.cc',
        'browser/prefs/pref_value_map_unittest.cc',
        'browser/prefs/pref_value_store_unittest.cc',
        'browser/prefs/proxy_config_dictionary_unittest.cc',
        'browser/prefs/proxy_policy_unittest.cc',
        'browser/prefs/proxy_prefs_unittest.cc',
        'browser/prefs/scoped_user_pref_update_unittest.cc',
        'browser/prefs/session_startup_pref_unittest.cc',
        'browser/prerender/prerender_history_unittest.cc',
        'browser/prerender/prerender_tracker_unittest.cc',
        'browser/prerender/prerender_unittest.cc',
        'browser/prerender/prerender_util_unittest.cc',
        'browser/printing/cloud_print/cloud_print_proxy_service_unittest.cc',
        'browser/printing/cloud_print/cloud_print_setup_source_unittest.cc',
        'browser/printing/print_dialog_cloud_unittest.cc',
        'browser/printing/print_job_unittest.cc',
        'browser/printing/print_preview_tab_controller_unittest.cc',
        'browser/printing/print_preview_unit_test_base.cc',
        'browser/printing/print_preview_unit_test_base.h',
        'browser/process_info_snapshot_mac_unittest.cc',
        'browser/process_singleton_linux_unittest.cc',
        'browser/process_singleton_mac_unittest.cc',
        'browser/profiles/avatar_menu_model_unittest.cc',
        'browser/profiles/gaia_info_update_service_unittest.cc',
        'browser/profiles/off_the_record_profile_impl_unittest.cc',
        'browser/profiles/profile_dependency_manager_unittest.cc',
        'browser/profiles/profile_downloader_unittest.cc',
        'browser/profiles/profile_info_cache_unittest.cc',
        'browser/profiles/profile_info_cache_unittest.h',
        'browser/profiles/profile_info_util_unittest.cc',
        'browser/profiles/profile_manager_unittest.cc',
        'browser/protector/composite_settings_change_unittest.cc',
        'browser/protector/homepage_change_unittest.cc',
        'browser/protector/prefs_backup_invalid_change_unittest.cc',
        'browser/protector/protected_prefs_watcher_unittest.cc',
        'browser/protector/session_startup_change_unittest.cc',
        'browser/renderer_host/web_cache_manager_unittest.cc',
        'browser/renderer_host/plugin_info_message_filter_unittest.cc',
        'browser/resources/print_preview/print_preview_utils.js',
        'browser/resources/print_preview/print_preview_utils_unittest.gtestjs',
        'browser/resources_util_unittest.cc',
        'browser/rlz/rlz_unittest.cc',
        'browser/safe_browsing/bloom_filter_unittest.cc',
        'browser/safe_browsing/browser_feature_extractor_unittest.cc',
        'browser/safe_browsing/chunk_range_unittest.cc',
        'browser/safe_browsing/client_side_detection_host_unittest.cc',
        'browser/safe_browsing/client_side_detection_service_unittest.cc',
        'browser/safe_browsing/download_protection_service_unittest.cc',
        'browser/safe_browsing/malware_details_unittest.cc',
        'browser/safe_browsing/prefix_set_unittest.cc',
        'browser/safe_browsing/protocol_manager_unittest.cc',
        'browser/safe_browsing/protocol_parser_unittest.cc',
        'browser/safe_browsing/safe_browsing_blocking_page_unittest.cc',
        'browser/safe_browsing/safe_browsing_database_unittest.cc',
        'browser/safe_browsing/safe_browsing_store_file_unittest.cc',
        'browser/safe_browsing/safe_browsing_store_unittest.cc',
        'browser/safe_browsing/safe_browsing_store_unittest_helper.cc',
        'browser/safe_browsing/safe_browsing_util_unittest.cc',
        'browser/safe_browsing/signature_util_win_unittest.cc',
        'browser/search_engines/search_host_to_urls_map_unittest.cc',
        'browser/search_engines/search_provider_install_data_unittest.cc',
        'browser/search_engines/template_url_fetcher_unittest.cc',
        'browser/search_engines/template_url_service_test_util.cc',
        'browser/search_engines/template_url_service_test_util.h',
        'browser/search_engines/template_url_service_sync_unittest.cc',
        'browser/search_engines/template_url_service_unittest.cc',
        'browser/search_engines/template_url_parser_unittest.cc',
        'browser/search_engines/template_url_prepopulate_data_unittest.cc',
        'browser/search_engines/template_url_scraper_unittest.cc',
        'browser/search_engines/template_url_unittest.cc',
        'browser/sessions/session_backend_unittest.cc',
        'browser/sessions/session_service_unittest.cc',
        'browser/shell_integration_unittest.cc',
        'browser/signin/signin_manager_unittest.cc',
        'browser/signin/signin_manager_fake.cc',
        'browser/signin/signin_manager_fake.h',
        'browser/signin/signin_tracker_unittest.cc',
        'browser/signin/token_service_unittest.cc',
        'browser/signin/token_service_unittest.h',
        'browser/signin/ubertoken_fetcher_unittest.cc',
        'browser/speech/extension_api/tts_extension_api_controller_unittest.cc',
        'browser/speech/speech_recognition_bubble_controller_unittest.cc',
        'browser/spellchecker/spellcheck_platform_mac_unittest.cc',
        'browser/spellchecker/spellcheck_profile_unittest.cc',
        'browser/spellchecker/spelling_service_client_unittest.cc',
        'browser/status_icons/status_icon_unittest.cc',
        'browser/status_icons/status_tray_unittest.cc',
        'browser/sync/abstract_profile_sync_service_test.cc',
        'browser/sync/abstract_profile_sync_service_test.h',
        'browser/sync/backend_migrator_unittest.cc',
        'browser/sync/glue/app_notification_data_type_controller_unittest.cc',
        'browser/sync/glue/autofill_data_type_controller_unittest.cc',
        'browser/sync/glue/bookmark_data_type_controller_unittest.cc',
        'browser/sync/glue/bridged_sync_notifier_unittest.cc',
        'browser/sync/glue/browser_thread_model_worker_unittest.cc',
        'browser/sync/glue/change_processor_mock.cc',
        'browser/sync/glue/change_processor_mock.h',
        'browser/sync/glue/chrome_encryptor_unittest.cc',
        'browser/sync/glue/chrome_extensions_activity_monitor_unittest.cc',
        'browser/sync/glue/chrome_sync_notification_bridge_unittest.cc',
        'browser/sync/glue/data_type_controller_mock.cc',
        'browser/sync/glue/data_type_controller_mock.h',
        'browser/sync/glue/data_type_error_handler_mock.cc',
        'browser/sync/glue/data_type_error_handler_mock.cc',
        'browser/sync/glue/data_type_manager_impl_unittest.cc',
        'browser/sync/glue/data_type_manager_mock.cc',
        'browser/sync/glue/data_type_manager_mock.h',
        'browser/sync/glue/fake_generic_change_processor.cc',
        'browser/sync/glue/fake_generic_change_processor.h',
        'browser/sync/glue/frontend_data_type_controller_mock.cc',
        'browser/sync/glue/frontend_data_type_controller_mock.h',
        'browser/sync/glue/frontend_data_type_controller_unittest.cc',
        'browser/sync/glue/http_bridge_unittest.cc',
        'browser/sync/glue/model_associator_mock.cc',
        'browser/sync/glue/model_associator_mock.h',
        'browser/sync/glue/new_non_frontend_data_type_controller_mock.cc',
        'browser/sync/glue/new_non_frontend_data_type_controller_mock.h',
        'browser/sync/glue/new_non_frontend_data_type_controller_unittest.cc',
        'browser/sync/glue/non_frontend_data_type_controller_mock.cc',
        'browser/sync/glue/non_frontend_data_type_controller_mock.h',
        'browser/sync/glue/non_frontend_data_type_controller_unittest.cc',
        'browser/sync/glue/search_engine_data_type_controller_unittest.cc',
        'browser/sync/glue/session_model_associator_unittest.cc',
        'browser/sync/glue/shared_change_processor_mock.cc',
        'browser/sync/glue/shared_change_processor_mock.h',
        'browser/sync/glue/shared_change_processor_unittest.cc',
        'browser/sync/glue/sync_backend_host_unittest.cc',
        'browser/sync/glue/sync_backend_registrar_unittest.cc',
        'browser/sync/glue/synced_session_tracker_unittest.cc',
        'browser/sync/glue/theme_data_type_controller_unittest.cc',
        'browser/sync/glue/theme_util_unittest.cc',
        'browser/sync/glue/typed_url_model_associator_unittest.cc',
        'browser/sync/glue/ui_data_type_controller_unittest.cc',
        'browser/sync/glue/ui_model_worker_unittest.cc',
        'browser/sync/profile_sync_components_factory_impl_unittest.cc',
        'browser/sync/profile_sync_components_factory_mock.cc',
        'browser/sync/profile_sync_components_factory_mock.h',
        'browser/sync/profile_sync_service_autofill_unittest.cc',
        'browser/sync/profile_sync_service_bookmark_unittest.cc',
        'browser/sync/profile_sync_service_password_unittest.cc',
        'browser/sync/profile_sync_service_preference_unittest.cc',
        'browser/sync/profile_sync_service_session_unittest.cc',
        'browser/sync/profile_sync_service_startup_unittest.cc',
        'browser/sync/profile_sync_service_typed_url_unittest.cc',
        'browser/sync/profile_sync_service_unittest.cc',
        'browser/sync/profile_sync_test_util.cc',
        'browser/sync/profile_sync_test_util.h',
        'browser/sync/sync_global_error_unittest.cc',
        'browser/sync/sync_prefs_unittest.cc',
        'browser/sync/sync_ui_util_mac_unittest.mm',
        'browser/sync/sync_ui_util_unittest.cc',
        'browser/sync/test_profile_sync_service.cc',
        'browser/sync/test_profile_sync_service.h',
        'browser/sync/test/test_http_bridge_factory.cc',
        'browser/sync/test/test_http_bridge_factory.h',
        'browser/tab_contents/render_view_context_menu_unittest.cc',
        'browser/tab_contents/thumbnail_generator_unittest.cc',
        'browser/task_manager/task_manager_unittest.cc',
        'browser/task_profiler/task_profiler_data_serializer_unittest.cc',
        'browser/themes/browser_theme_pack_unittest.cc',
        'browser/themes/theme_service_unittest.cc',
        'browser/ui/browser_list_unittest.cc',
        'browser/ui/browser_unittest.cc',
        # It is safe to list */cocoa/* files in the "common" file list
        # without an explicit exclusion since gyp is smart enough to
        # exclude them from non-Mac builds.
        'browser/ui/cocoa/about_ipc_controller_unittest.mm',
        'browser/ui/cocoa/accelerators_cocoa_unittest.mm',
        'browser/ui/cocoa/animatable_image_unittest.mm',
        'browser/ui/cocoa/animatable_view_unittest.mm',
        'browser/ui/cocoa/applescript/bookmark_applescript_utils_unittest.h',
        'browser/ui/cocoa/applescript/bookmark_applescript_utils_unittest.mm',
        'browser/ui/cocoa/applescript/bookmark_folder_applescript_unittest.mm',
        'browser/ui/cocoa/applescript/bookmark_item_applescript_unittest.mm',
        'browser/ui/cocoa/background_gradient_view_unittest.mm',
        'browser/ui/cocoa/background_tile_view_unittest.mm',
        'browser/ui/cocoa/base_bubble_controller_unittest.mm',
        'browser/ui/cocoa/bookmarks/bookmark_all_tabs_controller_unittest.mm',
        'browser/ui/cocoa/bookmarks/bookmark_bar_bridge_unittest.mm',
        'browser/ui/cocoa/bookmarks/bookmark_bar_controller_unittest.mm',
        'browser/ui/cocoa/bookmarks/bookmark_bar_folder_button_cell_unittest.mm',
        'browser/ui/cocoa/bookmarks/bookmark_bar_folder_controller_unittest.mm',
        'browser/ui/cocoa/bookmarks/bookmark_bar_folder_hover_state_unittest.mm',
        'browser/ui/cocoa/bookmarks/bookmark_bar_folder_view_unittest.mm',
        'browser/ui/cocoa/bookmarks/bookmark_bar_folder_window_unittest.mm',
        'browser/ui/cocoa/bookmarks/bookmark_bar_toolbar_view_unittest.mm',
        'browser/ui/cocoa/bookmarks/bookmark_bar_unittest_helper.h',
        'browser/ui/cocoa/bookmarks/bookmark_bar_unittest_helper.mm',
        'browser/ui/cocoa/bookmarks/bookmark_bar_view_unittest.mm',
        'browser/ui/cocoa/bookmarks/bookmark_bubble_controller_unittest.mm',
        'browser/ui/cocoa/bookmarks/bookmark_button_cell_unittest.mm',
        'browser/ui/cocoa/bookmarks/bookmark_button_unittest.mm',
        'browser/ui/cocoa/bookmarks/bookmark_editor_base_controller_unittest.mm',
        'browser/ui/cocoa/bookmarks/bookmark_editor_controller_unittest.mm',
        'browser/ui/cocoa/bookmarks/bookmark_folder_target_unittest.mm',
        'browser/ui/cocoa/bookmarks/bookmark_menu_bridge_unittest.mm',
        'browser/ui/cocoa/bookmarks/bookmark_menu_cocoa_controller_unittest.mm',
        'browser/ui/cocoa/bookmarks/bookmark_menu_unittest.mm',
        'browser/ui/cocoa/bookmarks/bookmark_model_observer_for_cocoa_unittest.mm',
        'browser/ui/cocoa/bookmarks/bookmark_name_folder_controller_unittest.mm',
        'browser/ui/cocoa/bookmarks/bookmark_tree_browser_cell_unittest.mm',
        'browser/ui/cocoa/browser/avatar_button_controller_unittest.mm',
        'browser/ui/cocoa/browser/avatar_menu_bubble_controller_unittest.mm',
        'browser/ui/cocoa/browser/edit_search_engine_cocoa_controller_unittest.mm',
        'browser/ui/cocoa/browser_frame_view_unittest.mm',
        'browser/ui/cocoa/browser_window_cocoa_unittest.mm',
        'browser/ui/cocoa/browser_window_controller_unittest.mm',
        'browser/ui/cocoa/bubble_view_unittest.mm',
        'browser/ui/cocoa/chrome_browser_window_unittest.mm',
        'browser/ui/cocoa/chrome_event_processing_window_unittest.mm',
        'browser/ui/cocoa/chrome_to_mobile_bubble_controller_unittest.mm',
        'browser/ui/cocoa/clickhold_button_cell_unittest.mm',
        'browser/ui/cocoa/cocoa_profile_test.h',
        'browser/ui/cocoa/cocoa_profile_test.mm',
        'browser/ui/cocoa/cocoa_test_helper.h',
        'browser/ui/cocoa/cocoa_test_helper.mm',
        'browser/ui/cocoa/command_observer_bridge_unittest.mm',
        'browser/ui/cocoa/confirm_bubble_controller_unittest.mm',
        'browser/ui/cocoa/confirm_quit_panel_controller_unittest.mm',
        'browser/ui/cocoa/content_settings/collected_cookies_mac_unittest.mm',
        'browser/ui/cocoa/content_settings/cookie_details_unittest.mm',
        'browser/ui/cocoa/content_settings/cookie_details_view_controller_unittest.mm',
        'browser/ui/cocoa/download/download_item_button_unittest.mm',
        'browser/ui/cocoa/download/download_shelf_mac_unittest.mm',
        'browser/ui/cocoa/download/download_shelf_view_unittest.mm',
        'browser/ui/cocoa/download/download_util_mac_unittest.mm',
        'browser/ui/cocoa/draggable_button_unittest.mm',
        'browser/ui/cocoa/event_utils_unittest.mm',
        'browser/ui/cocoa/extensions/browser_actions_container_view_unittest.mm',
        'browser/ui/cocoa/extensions/chevron_menu_button_unittest.mm',
        'browser/ui/cocoa/extensions/extension_install_dialog_controller_unittest.mm',
        'browser/ui/cocoa/extensions/extension_installed_bubble_controller_unittest.mm',
        'browser/ui/cocoa/extensions/extension_popup_controller_unittest.mm',
        'browser/ui/cocoa/fast_resize_view_unittest.mm',
        'browser/ui/cocoa/find_bar/find_bar_bridge_unittest.mm',
        'browser/ui/cocoa/find_bar/find_bar_cocoa_controller_unittest.mm',
        'browser/ui/cocoa/find_bar/find_bar_text_field_cell_unittest.mm',
        'browser/ui/cocoa/find_bar/find_bar_text_field_unittest.mm',
        'browser/ui/cocoa/find_bar/find_bar_view_unittest.mm',
        'browser/ui/cocoa/find_pasteboard_unittest.mm',
        'browser/ui/cocoa/first_run_bubble_controller_unittest.mm',
        'browser/ui/cocoa/floating_bar_backing_view_unittest.mm',
        'browser/ui/cocoa/framed_browser_window_unittest.mm',
        'browser/ui/cocoa/fullscreen_window_unittest.mm',
        'browser/ui/cocoa/fullscreen_exit_bubble_controller_unittest.mm',
        'browser/ui/cocoa/gradient_button_cell_unittest.mm',
        'browser/ui/cocoa/history_menu_bridge_unittest.mm',
        'browser/ui/cocoa/history_menu_cocoa_controller_unittest.mm',
        'browser/ui/cocoa/hover_image_button_unittest.mm',
        'browser/ui/cocoa/hung_renderer_controller_unittest.mm',
        'browser/ui/cocoa/hyperlink_button_cell_unittest.mm',
        'browser/ui/cocoa/hyperlink_text_view_unittest.mm',
        'browser/ui/cocoa/image_button_cell_unittest.mm',
        'browser/ui/cocoa/image_utils_unittest.mm',
        'browser/ui/cocoa/info_bubble_view_unittest.mm',
        'browser/ui/cocoa/info_bubble_window_unittest.mm',
        'browser/ui/cocoa/infobars/infobar_container_controller_unittest.mm',
        'browser/ui/cocoa/infobars/infobar_controller_unittest.mm',
        'browser/ui/cocoa/infobars/infobar_gradient_view_unittest.mm',
        'browser/ui/cocoa/infobars/translate_infobar_unittest.mm',
        'browser/ui/cocoa/location_bar/autocomplete_text_field_cell_unittest.mm',
        'browser/ui/cocoa/location_bar/autocomplete_text_field_editor_unittest.mm',
        'browser/ui/cocoa/location_bar/autocomplete_text_field_unittest.mm',
        'browser/ui/cocoa/location_bar/autocomplete_text_field_unittest_helper.mm',
        'browser/ui/cocoa/location_bar/ev_bubble_decoration_unittest.mm',
        'browser/ui/cocoa/location_bar/image_decoration_unittest.mm',
        'browser/ui/cocoa/location_bar/keyword_hint_decoration_unittest.mm',
        'browser/ui/cocoa/location_bar/omnibox_popup_view_unittest.mm',
        'browser/ui/cocoa/location_bar/selected_keyword_decoration_unittest.mm',
        'browser/ui/cocoa/menu_button_unittest.mm',
        'browser/ui/cocoa/menu_controller_unittest.mm',
        'browser/ui/cocoa/notifications/balloon_controller_unittest.mm',
        'browser/ui/cocoa/nsimage_cache_unittest.mm',
        'browser/ui/cocoa/nsmenuitem_additions_unittest.mm',
        'browser/ui/cocoa/omnibox/omnibox_popup_view_mac_unittest.mm',
        'browser/ui/cocoa/omnibox/omnibox_view_mac_unittest.mm',
        'browser/ui/cocoa/one_click_signin_bubble_controller_unittest.mm',
        'browser/ui/cocoa/one_click_signin_dialog_controller_unittest.mm',
        'browser/ui/cocoa/page_info_bubble_controller_unittest.mm',
        'browser/ui/cocoa/profile_menu_controller_unittest.mm',
        'browser/ui/cocoa/run_loop_testing_unittest.mm',
        'browser/ui/cocoa/status_bubble_mac_unittest.mm',
        'browser/ui/cocoa/status_icons/status_icon_mac_unittest.mm',
        'browser/ui/cocoa/styled_text_field_cell_unittest.mm',
        'browser/ui/cocoa/styled_text_field_test_helper.h',
        'browser/ui/cocoa/styled_text_field_test_helper.mm',
        'browser/ui/cocoa/styled_text_field_unittest.mm',
        'browser/ui/cocoa/tab_contents/previewable_contents_controller_unittest.mm',
        'browser/ui/cocoa/tab_contents/sad_tab_controller_unittest.mm',
        'browser/ui/cocoa/tab_contents/sad_tab_view_unittest.mm',
        'browser/ui/cocoa/tab_view_picker_table_unittest.mm',
        'browser/ui/cocoa/table_row_nsimage_cache_unittest.mm',
        'browser/ui/cocoa/tabpose_window_unittest.mm',
        'browser/ui/cocoa/tabs/tab_controller_unittest.mm',
        'browser/ui/cocoa/tabs/tab_strip_controller_unittest.mm',
        'browser/ui/cocoa/tabs/tab_strip_view_unittest.mm',
        'browser/ui/cocoa/tabs/tab_view_unittest.mm',
        'browser/ui/cocoa/tabs/throbber_view_unittest.mm',
        'browser/ui/cocoa/task_manager_mac_unittest.mm',
        'browser/ui/cocoa/toolbar/reload_button_unittest.mm',
        'browser/ui/cocoa/toolbar/toolbar_button_unittest.mm',
        'browser/ui/cocoa/toolbar/toolbar_controller_unittest.mm',
        'browser/ui/cocoa/toolbar/toolbar_view_unittest.mm',
        'browser/ui/cocoa/tracking_area_unittest.mm',
        'browser/ui/cocoa/vertical_gradient_view_unittest.mm',
        'browser/ui/cocoa/view_resizer_pong.h',
        'browser/ui/cocoa/view_resizer_pong.mm',
        'browser/ui/cocoa/web_dialog_window_controller_unittest.mm',
        'browser/ui/cocoa/web_intent_sheet_controller_unittest.mm',
        'browser/ui/cocoa/window_size_autosaver_unittest.mm',
        'browser/ui/cocoa/wrench_menu/menu_tracked_root_view_unittest.mm',
        'browser/ui/cocoa/wrench_menu/wrench_menu_button_cell_unittest.mm',
        'browser/ui/cocoa/wrench_menu/wrench_menu_controller_unittest.mm',
        'browser/ui/constrained_window_tab_helper_unittest.cc',
        'browser/ui/content_settings/content_setting_bubble_model_unittest.cc',
        'browser/ui/content_settings/content_setting_image_model_unittest.cc',
        'browser/ui/find_bar/find_backend_unittest.cc',
        'browser/ui/global_error_service_unittest.cc',
        'browser/ui/gtk/accelerators_gtk_unittest.cc',
        'browser/ui/gtk/bookmarks/bookmark_bar_gtk_unittest.cc',
        'browser/ui/gtk/bookmarks/bookmark_editor_gtk_unittest.cc',
        'browser/ui/gtk/bookmarks/bookmark_utils_gtk_unittest.cc',
        'browser/ui/gtk/gtk_chrome_shrinkable_hbox_unittest.cc',
        'browser/ui/gtk/gtk_theme_service_unittest.cc',
        'browser/ui/gtk/gtk_util_unittest.cc',
        'browser/ui/gtk/omnibox/omnibox_popup_view_gtk_unittest.cc',
        'browser/ui/gtk/one_click_signin_dialog_gtk_unittest.cc',
        'browser/ui/gtk/reload_button_gtk_unittest.cc',
        'browser/ui/gtk/status_icons/status_tray_gtk_unittest.cc',
        'browser/ui/gtk/tabs/tab_renderer_gtk_unittest.cc',
        'browser/ui/intents/web_intent_inline_disposition_delegate_unittest.cc',
        'browser/ui/intents/web_intent_picker_model_unittest.cc',
        'browser/ui/intents/web_intent_picker_unittest.cc',
        'browser/ui/intents/web_intents_model_unittest.cc',
        'browser/ui/login/login_prompt_unittest.cc',
        'browser/ui/omnibox/omnibox_view_unittest.cc',
        'browser/ui/panels/display_settings_provider_win_unittest.cc',
        'browser/ui/panels/panel_browser_window_cocoa_unittest.mm',
        'browser/ui/panels/panel_mouse_watcher_unittest.cc',
        'browser/ui/search_engines/keyword_editor_controller_unittest.cc',
        'browser/ui/select_file_dialog_unittest.cc',
        'browser/ui/tabs/dock_info_unittest.cc',
        'browser/ui/tabs/pinned_tab_codec_unittest.cc',
        'browser/ui/tabs/pinned_tab_service_unittest.cc',
        'browser/ui/tabs/pinned_tab_test_utils.cc',
        'browser/ui/tabs/tab_menu_model_unittest.cc',
        'browser/ui/tabs/tab_strip_model_unittest.cc',
        'browser/ui/tabs/tab_strip_selection_model_unittest.cc',
        'browser/ui/tabs/test_tab_strip_model_delegate.cc',
        'browser/ui/tabs/test_tab_strip_model_delegate.h',
        'browser/ui/tests/ui_gfx_image_unittest.cc',
        'browser/ui/tests/ui_gfx_image_unittest.mm',
        'browser/ui/toolbar/back_forward_menu_model_unittest.cc',
        'browser/ui/toolbar/encoding_menu_controller_unittest.cc',
        'browser/ui/toolbar/toolbar_model_unittest.cc',
        'browser/ui/toolbar/wrench_menu_model_unittest.cc',
        'browser/ui/views/accelerator_table_unittest.cc',
        'browser/ui/views/accessibility_event_router_views_unittest.cc',
        'browser/ui/views/ash/app_list/app_list_model_builder_unittest.cc',
        'browser/ui/views/ash/key_rewriter_unittest.cc',
        'browser/ui/views/ash/launcher/browser_launcher_item_controller_unittest.cc',
        'browser/ui/views/ash/launcher/chrome_launcher_controller_unittest.cc',
        'browser/ui/views/ash/window_positioner_unittest.cc',
        'browser/ui/views/bookmarks/bookmark_context_menu_test.cc',
        'browser/ui/views/bookmarks/bookmark_editor_view_unittest.cc',
        'browser/ui/views/crypto_module_password_dialog_view_unittest.cc',
        'browser/ui/views/extensions/browser_action_drag_data_unittest.cc',
        'browser/ui/views/first_run_bubble_unittest.cc',
        'browser/ui/views/reload_button_unittest.cc',
        'browser/ui/views/select_file_dialog_extension_unittest.cc',
        'browser/ui/views/select_file_dialog_win_unittest.cc',
        'browser/ui/views/status_icons/status_tray_win_unittest.cc',
        'browser/ui/views/tabs/fake_base_tab_strip_controller.cc',
        'browser/ui/views/tabs/fake_base_tab_strip_controller.h',
        'browser/ui/views/tabs/tab_unittest.cc',
        'browser/ui/views/tabs/tab_strip_unittest.cc',
        'browser/ui/views/tabs/touch_tab_strip_layout_unittest.cc',
        'browser/ui/webui/chrome_web_ui_data_source_unittest.cc',
        'browser/ui/webui/fileicon_source_unittest.cc',
        'browser/ui/webui/ntp/suggestions_combiner_unittest.cc',
        'browser/ui/webui/options2/chromeos/virtual_keyboard_manager_handler2_unittest.cc',
        'browser/ui/webui/options2/language_options_handler2_unittest.cc',
        'browser/ui/webui/policy_ui_unittest.cc',
        'browser/ui/webui/print_preview/print_preview_handler_unittest.cc',
        'browser/ui/webui/print_preview/print_preview_ui_unittest.cc',
        'browser/ui/webui/sync_internals_ui_unittest.cc',
        'browser/ui/webui/sync_setup_handler_unittest.cc',
        'browser/ui/webui/theme_source_unittest.cc',
        'browser/ui/webui/web_dialog_web_contents_delegate_unittest.cc',
        'browser/ui/webui/web_ui_unittest.cc',
        'browser/ui/window_sizer_ash_unittest.cc',
        'browser/ui/window_sizer_unittest.cc',
        'browser/ui/window_snapshot/window_snapshot_mac_unittest.mm',
        'browser/chrome_to_mobile_service_unittest.cc',
        'browser/user_style_sheet_watcher_unittest.cc',
        'browser/visitedlink/visitedlink_unittest.cc',
        'browser/web_applications/web_app_mac_unittest.mm',
        'browser/web_applications/web_app_unittest.cc',
        'browser/web_resource/promo_resource_service_unittest.cc',
        'browser/webdata/autofill_entry_unittest.cc',
        'browser/webdata/autofill_profile_syncable_service_unittest.cc',
        'browser/webdata/autofill_table_unittest.cc',
        'browser/webdata/keyword_table_unittest.cc',
        'browser/webdata/token_service_table_unittest.cc',
        'browser/webdata/web_apps_table_unittest.cc',
        'browser/webdata/web_data_service_test_util.h',
        'browser/webdata/web_data_service_unittest.cc',
        'browser/webdata/web_database_migration_unittest.cc',
        'browser/webdata/web_intents_table_unittest.cc',
        'browser/website_settings_unittest.cc',
        'common/bzip2_unittest.cc',
        'common/child_process_logging_mac_unittest.mm',
        'common/chrome_paths_unittest.cc',
        'common/common_param_traits_unittest.cc',
        'common/content_settings_helper_unittest.cc',
        'common/content_settings_pattern_parser_unittest.cc',
        'common/content_settings_pattern_unittest.cc',
        'common/extensions/csp_validator_unittest.cc',
        'common/extensions/extension_action_unittest.cc',
        'common/extensions/extension_commands_unittest.cc',
        'common/extensions/extension_constants_unittest.cc',
        'common/extensions/extension_file_util_unittest.cc',
        'common/extensions/extension_icon_set_unittest.cc',
        'common/extensions/extension_l10n_util_unittest.cc',
        'common/extensions/extension_localization_peer_unittest.cc',
        'common/extensions/extension_message_bundle_unittest.cc',
        'common/extensions/extension_permission_set_unittest.cc',
        'common/extensions/extension_resource_unittest.cc',
        'common/extensions/extension_set_unittest.cc',
        'common/extensions/extension_test_util.h',
        'common/extensions/extension_test_util.cc',
        'common/extensions/extension_unittest.cc',
        'common/extensions/extension_unpacker_unittest.cc',
        'common/extensions/feature_unittest.cc',
        'common/extensions/manifest_tests/extension_manifest_test.cc',
        'common/extensions/manifest_tests/extension_manifests_background_unittest.cc',
        'common/extensions/manifest_tests/extension_manifests_chromepermission_unittest.cc',
        'common/extensions/manifest_tests/extension_manifests_contentscript_unittest.cc',
        'common/extensions/manifest_tests/extension_manifests_default_unittest.cc',
        'common/extensions/manifest_tests/extension_manifests_devtools_unittest.cc',
        'common/extensions/manifest_tests/extension_manifests_excludematches_unittest.cc',
        'common/extensions/manifest_tests/extension_manifests_experimental_unittest.cc',
        'common/extensions/manifest_tests/extension_manifests_filebrowser_unittest.cc',
        'common/extensions/manifest_tests/extension_manifests_homepage_unittest.cc',
        'common/extensions/manifest_tests/extension_manifests_icon_unittest.cc',
        'common/extensions/manifest_tests/extension_manifests_initvalue_unittest.cc',
        'common/extensions/manifest_tests/extension_manifests_isolatedapp_unittest.cc',
        'common/extensions/manifest_tests/extension_manifests_launch_unittest.cc',
        'common/extensions/manifest_tests/extension_manifests_offline_unittest.cc',
        'common/extensions/manifest_tests/extension_manifests_old_unittest.cc',
        'common/extensions/manifest_tests/extension_manifests_options_unittest.cc',
        'common/extensions/manifest_tests/extension_manifests_override_unittest.cc',
        'common/extensions/manifest_tests/extension_manifests_pageaction_unittest.cc',
        'common/extensions/manifest_tests/extension_manifests_platformapp_unittest.cc',
        'common/extensions/manifest_tests/extension_manifests_portsinpermissions_unittest.cc',
        'common/extensions/manifest_tests/extension_manifests_storage_unittest.cc',
        'common/extensions/manifest_tests/extension_manifests_tts_unittest.cc',
        'common/extensions/manifest_tests/extension_manifests_ui_unittest.cc',
        'common/extensions/manifest_tests/extension_manifests_update_unittest.cc',
        'common/extensions/manifest_tests/extension_manifests_validapp_unittest.cc',
        'common/extensions/manifest_tests/extension_manifests_web_unittest.cc',
        'common/extensions/matcher/substring_set_matcher_unittest.cc',
        'common/extensions/matcher/url_matcher_unittest.cc',
        'common/extensions/matcher/url_matcher_factory_unittest.cc',
        'common/extensions/manifest_unittest.cc',
        'common/extensions/simple_feature_provider_unittest.cc',
        'common/extensions/update_manifest_unittest.cc',
        'common/extensions/url_pattern_set_unittest.cc',
        'common/extensions/url_pattern_unittest.cc',
        'common/extensions/user_script_unittest.cc',
        'common/extensions/api/extension_api_unittest.cc',
        'common/guid_unittest.cc',
        'common/important_file_writer_unittest.cc',
        'common/json_pref_store_unittest.cc',
        'common/json_schema_validator_unittest.cc',
        'common/json_schema_validator_unittest_base.cc',
        'common/json_schema_validator_unittest_base.h',
        'common/json_value_serializer_unittest.cc',
        'common/mac/cfbundle_blocker_unittest.mm',
        'common/mac/mock_launchd.cc',
        'common/mac/mock_launchd.h',
        'common/mac/objc_method_swizzle_unittest.mm',
        'common/mac/objc_zombie_unittest.mm',
        'common/metrics/experiments_helper_unittest.cc',
        'common/metrics/metrics_log_base_unittest.cc',
        'common/metrics/metrics_log_manager_unittest.cc',
        'common/multi_process_lock_unittest.cc',
        'common/net/url_util_unittest.cc',
        'common/net/gaia/gaia_auth_fetcher_unittest.cc',
        'common/net/gaia/gaia_authenticator_unittest.cc',
        'common/net/gaia/gaia_oauth_client_unittest.cc',
        'common/net/gaia/google_service_auth_error_unittest.cc',
        'common/net/gaia/oauth_request_signer_unittest.cc',
        'common/net/gaia/oauth2_access_token_fetcher_unittest.cc',
        'common/net/gaia/oauth2_api_call_flow_unittest.cc',
        'common/net/gaia/oauth2_mint_token_fetcher_unittest.cc',
        'common/net/gaia/oauth2_mint_token_flow_unittest.cc',
        'common/net/gaia/oauth2_revocation_fetcher_unittest.cc',
        'common/net/x509_certificate_model_unittest.cc',
        'common/service_process_util_unittest.cc',
        'common/string_ordinal_unittest.cc',
        'common/switch_utils_unittest.cc',
        'common/thumbnail_score_unittest.cc',
        'common/time_format_unittest.cc',
        'common/web_apps_unittest.cc',
        'common/worker_thread_ticker_unittest.cc',
        'common/zip_reader_unittest.cc',
        'common/zip_unittest.cc',
        'nacl/nacl_ipc_adapter_unittest.cc',
        'nacl/nacl_validation_query_unittest.cc',
        'renderer/chrome_content_renderer_client_unittest.cc',
        'renderer/content_settings_observer_unittest.cc',
        'renderer/extensions/chrome_v8_context_set_unittest.cc',
        'renderer/extensions/event_unittest.cc',
        'renderer/extensions/json_schema_unittest.cc',
        'renderer/module_system_unittest.cc',
        'renderer/net/predictor_queue_unittest.cc',
        'renderer/net/renderer_predictor_unittest.cc',
        'renderer/plugins/plugin_uma_unittest.cc',
        'renderer/safe_browsing/features_unittest.cc',
        'renderer/safe_browsing/murmurhash3_util_unittest.cc',
        'renderer/safe_browsing/phishing_term_feature_extractor_unittest.cc',
        'renderer/safe_browsing/phishing_url_feature_extractor_unittest.cc',
        'renderer/safe_browsing/scorer_unittest.cc',
        'renderer/spellchecker/spellcheck_provider_hunspell_unittest.cc',
        'renderer/spellchecker/spellcheck_provider_mac_unittest.cc',
        'renderer/spellchecker/spellcheck_unittest.cc',
        'renderer/spellchecker/spellcheck_worditerator_unittest.cc',
        'service/cloud_print/cloud_print_helpers_unittest.cc',
        'service/cloud_print/cloud_print_token_store_unittest.cc',
        'service/cloud_print/cloud_print_url_fetcher_unittest.cc',
        'service/service_process_unittest.cc',
        'test/base/browser_with_test_window_test.cc',
        'test/base/browser_with_test_window_test.h',
        'test/base/chrome_render_view_test.cc',
        'test/base/chrome_render_view_test.h',
        'test/base/menu_model_test.cc',
        'test/base/menu_model_test.h',
        'test/base/v8_unit_test.cc',
        'test/base/v8_unit_test.h',
        'test/data/resource.rc',
        'test/data/unit/framework_unittest.gtestjs',
        'test/logging/win/mof_data_parser_unittest.cc',
        'tools/convert_dict/convert_dict_unittest.cc',
        '../ash/test/test_launcher_delegate.cc',
        '../ash/test/test_launcher_delegate.h',
        '../ash/test/test_shell_delegate.cc',
        '../testing/gtest_mac_unittest.mm',
        '../third_party/cld/encodings/compact_lang_det/compact_lang_det_unittest_small.cc',
        '../tools/json_schema_compiler/test/additional_properties_unittest.cc',
        '../tools/json_schema_compiler/test/any_unittest.cc',
        '../tools/json_schema_compiler/test/arrays_unittest.cc',
        '../tools/json_schema_compiler/test/choices_unittest.cc',
        '../tools/json_schema_compiler/test/crossref_unittest.cc',
        '../tools/json_schema_compiler/test/enums_unittest.cc',
        '../tools/json_schema_compiler/test/functions_on_types_unittest.cc',
        '../tools/json_schema_compiler/test/idl_schemas_unittest.cc',
        '../tools/json_schema_compiler/test/objects_unittest.cc',
        '../tools/json_schema_compiler/test/simple_api_unittest.cc',
        '../ui/views/test/test_views_delegate.cc',
        '../ui/views/test/test_views_delegate.h',
        '../ui/views/test/views_test_base.cc',
        '../ui/views/test/views_test_base.h',
        '../webkit/blob/local_file_reader_unittest.cc',
        '../webkit/fileapi/file_system_dir_url_request_job_unittest.cc',
        '../webkit/fileapi/file_system_operation_write_unittest.cc',
        '../webkit/fileapi/file_system_test_helper.cc',
        '../webkit/fileapi/file_system_test_helper.h',
        '../webkit/fileapi/file_system_url_request_job_unittest.cc',
        '../webkit/fileapi/file_writer_delegate_unittest.cc',
        '../webkit/fileapi/local_file_writer_unittest.cc',
        '../webkit/fileapi/mock_file_system_options.cc',
        '../webkit/fileapi/mock_file_system_options.h',
        '../webkit/quota/mock_storage_client.cc',
        '../webkit/quota/mock_storage_client.h',
      ],
      'conditions': [
        ['enable_background==0', {
          'sources/': [
            ['exclude', '^browser/background/'],
          ],
        }],
        ['enable_one_click_signin==0', {
          'sources!': [
            'browser/ui/cocoa/one_click_signin_bubble_controller_unittest.mm',
            'browser/ui/cocoa/one_click_signin_dialog_controller_unittest.mm',
            'browser/ui/gtk/one_click_signin_dialog_gtk_unittest.cc',
          ]
        }],
        ['enable_promo_resource_service==0', {
          'sources/': [
            ['exclude', '^browser/web_resource/promo_resource_service'],
          ],
        }],
        ['disable_nacl==1', {
          'sources!':[
            'nacl/nacl_ipc_adapter_unittest.cc',
            'nacl/nacl_validation_query_unittest.cc',
          ],
        }],
        ['target_arch!="arm"', {
          'dependencies': [
            # build time dependency.
            '../v8/tools/gyp/v8.gyp:v8_shell#host',
          ],
        }],
        ['enable_extensions==0', {
          'sources/': [
            ['exclude', '^browser/extensions/api/'],
            ['exclude', '^browser/extensions/extension_idle_api_unittest.cc'],
            ['exclude', '^browser/sync/glue/chrome_extensions_activity_monitor_unittest.cc'],
          ],
        }],
        ['use_aura==1', {
          'dependencies': [
            '../ui/aura/aura.gyp:test_support_aura',
          ],
          'sources/': [
            ['exclude', '^browser/automation/automation_provider_unittest.cc'],
            ['exclude', '^browser/accessibility/browser_accessibility_win_unittest.cc'],
            ['exclude', '^browser/ui/views/extensions/browser_action_drag_data_unittest.cc'],
            ['exclude', '^browser/ui/views/select_file_dialog_win_unittest.cc'],
            ['exclude', '^browser/ui/views/bookmarks/bookmark_editor_view_unittest.cc'],
            ['exclude', '^browser/ui/panels/display_settings_provider_win_unittest.cc'],
            ['exclude', '^browser/bookmarks/bookmark_node_data_unittest.cc'],
            ['exclude', '^browser/ui/window_sizer_unittest.cc'],
          ],
          'sources': [
            '../ash/test/ash_test_base.cc',
            '../ash/test/ash_test_base.h',
          ],
        }],
        ['enable_task_manager==0', {
          'sources/': [
            ['exclude', '^browser/task_manager/'],
            ['exclude', '^browser/ui/webui/task_manager/'],
          ],
        }],
        ['file_manager_extension==0', {
          'sources!': [
            'browser/ui/views/select_file_dialog_extension_unittest.cc',
          ],
        }],
        ['configuration_policy==0', {
          'sources!': [
            'browser/prefs/proxy_policy_unittest.cc',
            'browser/ui/webui/policy_ui_unittest.cc',
          ],
          'sources/': [
            ['exclude', '^browser/policy/'],
          ],
        }],
        ['input_speech==0', {
          'sources/': [
            ['exclude', '^browser/speech/'],
          ],
        }],
        ['notifications==0', {
          'sources/': [
            ['exclude', '^browser/notifications/'],
          ],
        }],
        ['enable_web_intents==0', {
          'sources/': [
            ['exclude', '^browser/intents'],
            ['exclude', '^browser/ui/intents'],
          ],
        }],
        ['safe_browsing==1', {
          'defines': [
            'ENABLE_SAFE_BROWSING',
          ],
        }, {  # safe_browsing == 0
          'sources!': [
            'browser/download/download_safe_browsing_client_unittest.cc',
          ],
          'sources/': [
            ['exclude', '^browser/safe_browsing/'],
            ['exclude', '^renderer/safe_browsing/'],
          ],
        }],
        ['enable_automation!=1', {
          'sources/': [
            ['exclude', '^browser/automation/'],
          ],
        }],
        ['use_virtual_keyboard!=1', {
          'sources/': [
            ['exclude', '^browser/chromeos/input_method/virtual_keyboard_selector_unittest.cc'],
            ['exclude', '^browser/ui/webui/options2/chromeos/virtual_keyboard_manager_handler2_unittest.cc'],
          ],
        }],
        ['enable_session_service!=1', {
          'sources!': [
            'browser/sessions/session_service_unittest.cc',
          ],
        }],
        ['chromeos==1', {
          'sources/': [
            ['exclude', '^browser/password_manager/native_backend_gnome_x_unittest.cc'],
            ['exclude', '^browser/password_manager/native_backend_kwallet_x_unittest.cc'],
            ['exclude', '^browser/safe_browsing/download_protection_service_unittest.cc' ],
          ],
          'sources': [
            'browser/ui/webui/feedback_ui_unittest.cc',
          ],
        }, { # else: chromeos == 0
          'sources/': [
            ['exclude', '^browser/chromeos/'],
            ['exclude', '^browser/policy/auto_enrollment_client_unittest.cc' ],
            ['exclude', '^browser/policy/configuration_policy_handler_chromeos_unittest.cc' ],
            ['exclude', '^browser/policy/cros_user_policy_cache_unittest.cc'],
            ['exclude', '^browser/policy/device_policy_cache_unittest.cc'],
            ['exclude', '^browser/policy/device_status_collector_unittest.cc'],
            ['exclude', '^browser/policy/enterprise_install_attributes_unittest.cc' ],
            ['exclude', '^browser/policy/network_configuration_updater_unittest.cc' ],
            ['exclude', '^browser/ui/webui/chromeos/imageburner/'],
            ['exclude', '^browser/ui/webui/chromeos/login'],
            ['exclude', '^browser/ui/webui/options/chromeos/'],
            ['exclude', '^browser/ui/webui/options2/chromeos/'],
          ],
        }],
        ['toolkit_uses_gtk == 1', {
          'conditions': [
            ['selinux==0', {
              'dependencies': [
                '../sandbox/sandbox.gyp:*',
              ],
            }],
            ['toolkit_views==1', {
              'sources!': [
                'browser/ui/gtk/accelerators_gtk_unittest.cc',
                'browser/ui/gtk/bookmarks/bookmark_bar_gtk_unittest.cc',
                'browser/ui/gtk/bookmarks/bookmark_editor_gtk_unittest.cc',
                'browser/ui/gtk/gtk_chrome_shrinkable_hbox_unittest.cc',
                'browser/ui/gtk/gtk_theme_service_unittest.cc',
                'browser/ui/gtk/omnibox/omnibox_popup_view_gtk_unittest.cc',
                'browser/ui/gtk/reload_button_gtk_unittest.cc',
                'browser/ui/gtk/status_icons/status_tray_gtk_unittest.cc',
              ],
            }],
            ['chromeos==0', {
              'conditions': [
                ['use_gnome_keyring==1', {
                  # We use a few library functions directly, so link directly.
                  'dependencies': [
                    '../build/linux/system.gyp:gnome_keyring_direct',
                  ],
                }],
              ],
            }],
          ],
          'dependencies': [
            '../build/linux/system.gyp:dbus',
            '../build/linux/system.gyp:gtk',
            '../dbus/dbus.gyp:dbus_test_support',
            '../tools/xdisplaycheck/xdisplaycheck.gyp:xdisplaycheck',
          ],
          'sources!': [
            'browser/printing/print_job_unittest.cc',
          ],
        }, { # else: toolkit_uses_gtk != 1
          'sources!': [
            'browser/ui/gtk/tabs/tab_renderer_gtk_unittest.cc',
            'browser/renderer_host/gtk_key_bindings_handler_unittest.cc',
            '../ui/views/focus/accelerator_handler_gtk_unittest.cc',
          ],
        }],
        ['toolkit_uses_gtk == 1 or chromeos==1 or (OS=="linux" and use_aura==1)', {
          'dependencies': [
            '../build/linux/system.gyp:ssl',
            '../third_party/mozc/chrome/chromeos/renderer/chromeos_renderer.gyp:mozc_candidates_proto',
          ],
        }],
        ['use_gnome_keyring == 0', {
          # Disable the GNOME Keyring tests if we are not using it.
          'sources!': [
            'browser/password_manager/native_backend_gnome_x_unittest.cc',
          ],
        }],
        ['OS=="linux" and use_aura==1', {
          'dependencies': [
            '../build/linux/system.gyp:dbus',
            '../dbus/dbus.gyp:dbus_test_support',
            '../ui/aura/aura.gyp:test_support_aura',
          ],
        }],
        ['os_posix == 1 and OS != "mac"', {
          'conditions': [
            ['linux_use_tcmalloc==1', {
              'dependencies': [
                '../base/allocator/allocator.gyp:allocator',
              ],
            }],
          ],
        }],
        ['OS=="mac"', {
           # The test fetches resources which means Mac need the app bundle to
           # exist on disk so it can pull from it.
          'dependencies': [
            'chrome',
            '../base/base.gyp:closure_blocks_leopard_compat',
            '../third_party/ocmock/ocmock.gyp:ocmock',
          ],
          'include_dirs': [
            '../third_party/GTM',
            '../third_party/GTM/AppKit',
          ],
          'sources!': [
            # Blocked on bookmark manager.
            'browser/bookmarks/bookmark_context_menu_controller_unittest.cc',
            'browser/ui/tabs/dock_info_unittest.cc',
            'browser/ui/tests/ui_gfx_image_unittest.cc',
            'browser/ui/gtk/reload_button_gtk_unittest.cc',
            'browser/password_manager/password_store_default_unittest.cc',
            'tools/convert_dict/convert_dict_unittest.cc',
            'renderer/spellchecker/spellcheck_provider_hunspell_unittest.cc',
          ],
          # TODO(mark): We really want this for all non-static library targets,
          # but when we tried to pull it up to the common.gypi level, it broke
          # other things like the ui, startup, and page_cycler tests. *shrug*
          'xcode_settings': {'OTHER_LDFLAGS': ['-Wl,-ObjC']},
        }, { # OS != "mac"
          'dependencies': [
            'chrome_resources.gyp:packed_extra_resources',
            'chrome_resources.gyp:packed_resources',
            'convert_dict_lib',
            '../third_party/hunspell/hunspell.gyp:hunspell',
          ],
          'sources!': [
            'browser/spellchecker/spellchecker_platform_engine_unittest.cc',
          ],
        }],
        ['OS!="win" and OS!="mac"', {
          'sources!': [
            'browser/rlz/rlz_unittest.cc',
          ],
        }],
        ['OS=="win"', {
          'dependencies': [
            'chrome_version_resources',
            'installer_util_strings',
            '../third_party/iaccessible2/iaccessible2.gyp:iaccessible2',
            '../third_party/isimpledom/isimpledom.gyp:isimpledom',
          ],
          'conditions': [
            ['win_use_allocator_shim==1', {
              'dependencies': [
                '<(allocator_target)',
              ],
            }],
          ],
          'include_dirs': [
            '<(DEPTH)/third_party/wtl/include',
          ],
          'sources': [
            # TODO:  It would be nice to have these pulled in
            # automatically from direct_dependent_settings in
            # their various targets (net.gyp:net_resources, etc.),
            # but that causes errors in other targets when
            # resulting .res files get referenced multiple times.
            '<(SHARED_INTERMEDIATE_DIR)/chrome/browser_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/common_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/extensions_api_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/renderer_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/theme_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/theme_resources_standard.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome_version/other_version.rc',
            '<(SHARED_INTERMEDIATE_DIR)/content/content_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/net/net_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/ui/ui_resources/ui_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/ui/ui_resources_standard/ui_resources_standard.rc',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/webkit_chromium_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/webkit_resources.rc',
          ],
          'link_settings': {
            'libraries': [
              '-lcomsupp.lib',
              '-loleacc.lib',
              '-lrpcrt4.lib',
              '-lurlmon.lib',
              '-lwinmm.lib',
            ],
          },
          'configurations': {
            'Debug_Base': {
              'msvs_settings': {
                'VCLinkerTool': {
                  # Forcing incremental build off to try to avoid incremental
                  # linking errors on 64-bit bots too. http://crbug.com/52555
                  'LinkIncremental': '1',
                },
              },
            },
          },
        }, { # else: OS != "win"
          'sources!': [
            'app/chrome_dll.rc',
            'browser/accessibility/browser_accessibility_win_unittest.cc',
            'browser/bookmarks/bookmark_node_data_unittest.cc',
            'browser/search_engines/template_url_scraper_unittest.cc',
            'browser/ui/views/bookmarks/bookmark_editor_view_unittest.cc',
            'browser/ui/views/extensions/browser_action_drag_data_unittest.cc',
            'browser/ui/views/first_run_search_engine_view_unittest.cc',
            'test/data/resource.rc',
          ],
        }],
        ['OS=="android"', {
          # TODO(yfriedman): Remove once unit_tests can link for Android.
          'type': '<(android_unit_test_target_type)',
          'sources!': [
            # Bookmark export/import are handled via the BookmarkColumns
            # ContentProvider.
            'browser/bookmarks/bookmark_html_writer_unittest.cc',

            'browser/bookmarks/bookmark_context_menu_controller_unittest.cc',
            'browser/shell_integration_unittest.cc',

            # No service process (which also requires multiprocess lock).
            'common/multi_process_lock_unittest.cc',

            # Sync setup uses native ui.
            'browser/ui/webui/sync_setup_handler_unittest.cc',

            # about:flags is unsupported.
            'browser/about_flags_unittest.cc',

            # There's no Browser/BrowserList on Android.
            'browser/browser_commands_unittest.cc',
            'browser/managed_mode_unittest.cc',
            'browser/net/gaia/gaia_oauth_fetcher_unittest.cc',
            'browser/profiles/off_the_record_profile_impl_unittest.cc',
            'browser/sync/profile_sync_service_session_unittest.cc',
            'browser/sync/sync_global_error_unittest.cc',
            'browser/sync/sync_setup_wizard_unittest.cc',
            'browser/ui/browser_list_unittest.cc',
            'browser/ui/browser_unittest.cc',
            'browser/ui/toolbar/toolbar_model_unittest.cc',
            'browser/ui/toolbar/wrench_menu_model_unittest.cc',
            'browser/ui/webui/web_dialog_web_contents_delegate_unittest.cc',
            'browser/ui/window_sizer_unittest.cc',
            'test/base/browser_with_test_window_test.cc',
            'test/base/browser_with_test_window_test.h',
            'test/base/test_browser_window.h',
          ],
          'sources/': [
            ['exclude', '^browser/captive_portal/'],
            ['exclude', '^browser/chrome_to_mobile'],
            ['exclude', '^browser/printing/'],
            ['exclude', '^browser/themes/'],
            ['exclude', '^browser/ui/panels'],
            ['exclude', '^browser/ui/tabs/'],
            ['exclude', '^browser/ui/toolbar/'],
            ['exclude', '^browser/ui/webui/options/'],
            ['exclude', '^browser/ui/webui/options2/'],
            ['exclude', '^browser/ui/webui/print_preview/'],
            # No service process on Android.
            ['exclude', '^common/service_'],
            ['exclude', '^service/'],
          ],
        }],
        ['enable_themes==0', {
          'sources!': [
            'browser/sync/glue/theme_data_type_controller_unittest.cc',
            'browser/sync/glue/theme_util_unittest.cc',
            'browser/ui/webui/theme_source_unittest.cc',
          ],
          'sources/': [
            ['exclude', '^browser/themes/'],
          ],
        }],
        ['enable_plugin_installation==0', {
          'sources!': [
            'browser/plugin_finder_unittest.cc',
          ],
        }],
        ['enable_protector_service==0', {
          'sources/': [
            ['exclude', '^browser/protector/'],
          ],
        }],
        ['toolkit_views==1', {
          'dependencies': [
            '../ui/views/views.gyp:views',
          ],
          'sources!': [
            'browser/ui/gtk/tabs/tab_renderer_gtk_unittest.cc',
          ],
        }, { # else: toolkit_views == 0
          'sources/': [
            ['exclude', '^browser/ui/views/'],
            ['exclude', '^../views/'],
            ['exclude', '^../ui/views/'],
            ['exclude', '^browser/extensions/key_identifier_conversion_views_unittest.cc'],
          ],
        }],
        ['use_nss==0 and use_openssl==0', {
          'sources!': [
            'common/net/x509_certificate_model_unittest.cc',
          ],
        }],
        ['use_openssl==1', {
          'sources/': [
            # OpenSSL build does not support firefox importer. See
            # http://crbug.com/64926
            ['exclude', '^browser/importer/'],
          ],
        }],
        ['use_cups==1', {
          'defines': [
            'USE_CUPS',
          ],
          'sources': [
            'browser/printing/print_system_task_proxy_unittest.cc',
          ],
        }],
        ['component=="shared_library" and incremental_chrome_dll!=1', {
          # This is needed for tests that subclass
          # RendererWebKitPlatformSupportImpl, which subclasses stuff in
          # glue, which refers to symbols defined in these files.
          # Hopefully this can be resolved with http://crbug.com/98755.
          'sources': [
            '../content/common/socket_stream_dispatcher.cc',
          ]},
        ],
      ],
    },
    {
      'target_name': 'chrome_app_unittests',
      'type': 'executable',
      'dependencies': [
        # unit tests should only depend on
        # 1) everything that the chrome binaries depend on:
        '<@(chromium_dependencies)',
        # 2) test-specific support libraries:
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        'test_support_common',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'app/breakpad_field_trial_win.cc',
        'app/breakpad_win.cc',
        'app/breakpad_unittest_win.cc',
        'app/hard_error_handler_win.cc',
        'app/run_all_unittests.cc'
      ],
      'conditions': [
        ['OS=="mac"', {
          # TODO(mark): We really want this for all non-static library targets,
          # but when we tried to pull it up to the common.gypi level, it broke
          # other things like the ui, startup, and page_cycler tests. *shrug*
          'xcode_settings': {'OTHER_LDFLAGS': ['-Wl,-ObjC']},
        }],
        ['OS=="win"', {
          'dependencies': [
            # breakpad is currently only tested on Windows.
            '../breakpad/breakpad.gyp:*',
          ],
          'conditions': [
            ['win_use_allocator_shim==1', {
              'dependencies': [
                '<(allocator_target)',
              ],
            }],
          ],
          'configurations': {
            'Debug_Base': {
              'msvs_settings': {
                'VCLinkerTool': {
                  # Forcing incremental build off to try to avoid incremental
                  # linking errors on 64-bit bots too. http://crbug.com/52555
                  'LinkIncremental': '1',
                },
              },
            },
          },
        }],
      ],
    },
    {
      'target_name': 'unit_tests_run',
      'type': 'none',
      'dependencies': [
        'unit_tests',
      ],
      'includes': [
        'unit_tests.isolate',
      ],
      'actions': [
        {
          'action_name': 'isolate',
          'inputs': [
            '<@(isolate_dependency_tracked)',
          ],
          'outputs': [
            '<(PRODUCT_DIR)/unit_tests.results',
          ],
          'action': [
            'python',
            '../tools/isolate/isolate.py',
            '--mode', '<(test_isolation_mode)',
            '--outdir', '<(test_isolation_outdir)',
            '--variable', 'PRODUCT_DIR', '<(PRODUCT_DIR)',
            '--variable', 'OS', '<(OS)',
            '--result', '<@(_outputs)',
            'unit_tests.isolate',
          ],
        },
      ],
    },
    {
      # Executable that runs each browser test in a new process.
      'target_name': 'browser_tests',
      'type': 'executable',
      'msvs_cygwin_shell': 0,
      'msvs_cygwin_dirs': ['<(DEPTH)/third_party/cygwin'],
      'copies': [
        {
          'destination': '<(PRODUCT_DIR)',
          'files': [
            # Keep 'test_case.html.mock-http-headers' with 'test_case.html'.
            '../ppapi/tests/test_case.html',
            '../ppapi/tests/test_case.html.mock-http-headers',
            '../ppapi/tests/test_page.css',
            '../ppapi/native_client/tests/ppapi_tests/test_case.nmf',
          ],
        },
        {
          'destination': '<(PRODUCT_DIR)/test_url_loader_data',
          'files': [
            '../ppapi/tests/test_url_loader_data/hello.txt',
          ],
        },
      ],
      'dependencies': [
        'browser',
        '../sync/protocol/sync_proto.gyp:sync_proto',
        'chrome_resources.gyp:chrome_resources',
        'chrome_resources.gyp:chrome_strings',
        'chrome_resources.gyp:packed_extra_resources',
        'chrome_resources.gyp:packed_resources',
        'common/extensions/api/api.gyp:api',
        'renderer',
        'test_support_common',
        '../base/base.gyp:base',
        '../base/base.gyp:base_i18n',
        '../base/base.gyp:test_support_base',
        '../net/net.gyp:net',
        '../net/net.gyp:net_test_support',
        '../skia/skia.gyp:skia',
        '../sync/sync.gyp:test_support_syncapi_service',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        '../third_party/cld/cld.gyp:cld',
        '../third_party/icu/icu.gyp:icui18n',
        '../third_party/icu/icu.gyp:icuuc',
        '../third_party/leveldatabase/leveldatabase.gyp:leveldatabase',
        '../v8/tools/gyp/v8.gyp:v8',
        '../webkit/webkit.gyp:test_shell_test_support',
        # Runtime dependencies
        '../ppapi/ppapi_internal.gyp:ppapi_tests',
        '../third_party/mesa/mesa.gyp:osmesa',
        '../webkit/webkit.gyp:pull_in_copy_TestNetscapePlugIn',
      ],
      'include_dirs': [
        '..',
      ],
      'defines': [
        'HAS_OUT_OF_PROC_TEST_RUNNER',
        'BROWSER_TESTS_HEADER_OVERRIDE="chrome/test/base/in_process_browser_test.h"',
      ],
      'sources': [
        'app/breakpad_mac_stubs.mm',
        'app/chrome_command_ids.h',
        'app/chrome_dll.rc',
        'app/chrome_dll_resource.h',
        'app/chrome_version.rc.version',
        'browser/accessibility/accessibility_extension_apitest.cc',
        'browser/app_controller_mac_browsertest.mm',
        'browser/autocomplete/autocomplete_browsertest.cc',
        'browser/autofill/autofill_browsertest.cc',
        'browser/autofill/form_structure_browsertest.cc',
        'browser/autofill/autofill_popup_view_browsertest.cc',
        'browser/automation/automation_tab_helper_browsertest.cc',
        'browser/bookmarks/bookmark_extension_apitest.cc',
        'browser/bookmarks/bookmark_manager_extension_apitest.cc',
        'browser/browser_encoding_browsertest.cc',
        'browser/browsing_data_database_helper_browsertest.cc',
        'browser/browsing_data_helper_browsertest.h',
        'browser/browsing_data_indexed_db_helper_browsertest.cc',
        'browser/browsing_data_local_storage_helper_browsertest.cc',
        'browser/chrome_main_browsertest.cc',
        'browser/chrome_switches_browsertest.cc',
        'browser/chromeos/cros/cros_in_process_browser_test.cc',
        'browser/chromeos/cros/cros_in_process_browser_test.h',
        'browser/chromeos/cros/cros_mock.cc',
        'browser/chromeos/cros/cros_mock.h',
        'browser/chromeos/disks/mock_disk_mount_manager.cc',
        'browser/chromeos/disks/mock_disk_mount_manager.h',
        'browser/chromeos/extensions/file_browser_notifications_browsertest.cc',
        'browser/chromeos/extensions/file_browser_private_apitest.cc',
        'browser/chromeos/extensions/echo_private_apitest.cc',
        'browser/chromeos/extensions/external_filesystem_apitest.cc',
        'browser/chromeos/gdata/gdata_documents_service_browsertest.cc',
        'browser/chromeos/gdata/mock_gdata_documents_service.cc',
        'browser/chromeos/gdata/mock_gdata_documents_service.h',
        'browser/chromeos/kiosk_mode/mock_kiosk_mode_settings.cc',
        'browser/chromeos/kiosk_mode/mock_kiosk_mode_settings.h',
        'browser/chromeos/login/enrollment/enterprise_enrollment_screen_browsertest.cc',
        'browser/chromeos/login/existing_user_controller_browsertest.cc',
        'browser/chromeos/login/login_browsertest.cc',
        'browser/chromeos/login/login_utils_browsertest.cc',
        'browser/chromeos/login/mock_authenticator.cc',
        'browser/chromeos/login/mock_authenticator.h',
        'browser/chromeos/login/enrollment/mock_enterprise_enrollment_screen.cc',
        'browser/chromeos/login/enrollment/mock_enterprise_enrollment_screen.h',
        'browser/chromeos/login/mock_eula_screen.cc',
        'browser/chromeos/login/mock_eula_screen.h',
        'browser/chromeos/login/mock_network_screen.cc',
        'browser/chromeos/login/mock_network_screen.h',
        'browser/chromeos/login/mock_screen_observer.cc',
        'browser/chromeos/login/mock_screen_observer.h',
        'browser/chromeos/login/mock_update_screen.cc',
        'browser/chromeos/login/mock_update_screen.h',
        'browser/chromeos/login/network_screen_browsertest.cc',
        'browser/chromeos/login/screen_locker_tester.cc',
        'browser/chromeos/login/screen_locker_tester.h',
        'browser/chromeos/login/update_screen_browsertest.cc',
        'browser/chromeos/login/wizard_controller_browsertest.cc',
        'browser/chromeos/login/wizard_in_process_browser_test.cc',
        'browser/chromeos/login/wizard_in_process_browser_test.h',
        'browser/chromeos/media/media_player_browsertest.cc',
        'browser/chromeos/oom_priority_manager_browsertest.cc',
        'browser/chromeos/process_proxy/process_proxy_browsertest.cc',
        'browser/chromeos/ui/idle_logout_dialog_view_browsertest.cc',
        'browser/collected_cookies_browsertest.cc',
        'browser/content_settings/content_settings_browsertest.cc',
        'browser/crash_recovery_browsertest.cc',
        'browser/custom_handlers/protocol_handler_registry_browsertest.cc',
        'browser/debugger/devtools_sanity_browsertest.cc',
        'browser/download/download_browsertest.cc',
        'browser/download/download_danger_prompt_browsertest.cc',
        'browser/download/download_extension_apitest.cc',
        'browser/download/download_extension_test.cc',
        'browser/download/download_query_unittest.cc',
        'browser/download/download_test_observer.cc',
        'browser/download/save_page_browsertest.cc',
        'browser/errorpage_browsertest.cc',
        'browser/extensions/alert_apitest.cc',
        'browser/extensions/all_urls_apitest.cc',
        'browser/extensions/api/app/app_apitest.cc',
        'browser/extensions/api/browsing_data/browsing_data_test.cc',
        'browser/extensions/api/cookies/cookies_apitest.cc',
        'browser/extensions/api/context_menu/context_menu_apitest.cc',
        'browser/extensions/api/content_settings/content_settings_apitest.cc',
        'browser/extensions/api/declarative/declarative_apitest.cc',
        'browser/extensions/api/dns/dns_apitest.cc',
        'browser/extensions/api/extension_action/browser_action_apitest.cc',
        'browser/extensions/api/extension_action/page_action_apitest.cc',
        'browser/extensions/api/extension_action/page_as_browser_action_apitest.cc',
        'browser/extensions/api/identity/identity_apitest.cc',
        'browser/extensions/api/idltest/idltest_apitest.cc',
        'browser/extensions/api/media_gallery/media_gallery_apitest.cc',
        'browser/extensions/api/offscreen_tabs/offscreen_tabs_apitest.cc',
        'browser/extensions/api/omnibox/omnibox_apitest.cc',
        'browser/extensions/api/permissions/permissions_apitest.cc',
        'browser/extensions/api/proxy/proxy_apitest.cc',
        'browser/extensions/api/serial/serial_apitest.cc',
        'browser/extensions/api/socket/socket_apitest.cc',
        'browser/extensions/api/terminal/terminal_private_apitest.cc',
        'browser/extensions/api/web_navigation/web_navigation_apitest.cc',
        'browser/extensions/api/web_request/web_request_apitest.cc',
        'browser/extensions/app_background_page_apitest.cc',
        'browser/extensions/app_notification_browsertest.cc',
        'browser/extensions/app_process_apitest.cc',
        'browser/extensions/autoupdate_interceptor.cc',
        'browser/extensions/autoupdate_interceptor.h',
        'browser/extensions/background_page_apitest.cc',
        'browser/extensions/background_scripts_apitest.cc',
        'browser/extensions/browser_action_test_util.h',
        'browser/extensions/browser_action_test_util_gtk.cc',
        'browser/extensions/browser_action_test_util_mac.mm',
        'browser/extensions/browser_action_test_util_views.cc',
        'browser/extensions/chrome_app_api_browsertest.cc',
        'browser/extensions/content_script_apitest.cc',
        'browser/extensions/content_security_policy_apitest.cc',
        'browser/extensions/convert_web_app_browsertest.cc',
        'browser/extensions/cross_origin_xhr_apitest.cc',
        'browser/extensions/crx_installer_browsertest.cc',
        'browser/extensions/events_apitest.cc',
        'browser/extensions/execute_script_apitest.cc',
        'browser/extensions/extension_apitest.cc',
        'browser/extensions/extension_apitest.h',
        'browser/extensions/extension_apitest_apitest.cc',
        'browser/extensions/extension_bindings_apitest.cc',
        'browser/extensions/extension_browsertest.cc',
        'browser/extensions/extension_browsertest.h',
        'browser/extensions/extension_browsertests_misc.cc',
        'browser/extensions/extension_chrome_auth_private_apitest.cc',
        'browser/extensions/extension_context_menu_browsertest.cc',
        'browser/extensions/extension_crash_recovery_browsertest.cc',
        'browser/extensions/extension_debugger_apitest.cc',
        'browser/extensions/extension_devtools_browsertest.cc',
        'browser/extensions/extension_devtools_browsertest.h',
        'browser/extensions/extension_devtools_browsertests.cc',
        'browser/extensions/extension_disabled_ui_browsertest.cc',
        'browser/extensions/extension_dom_clipboard_apitest.cc',
        'browser/extensions/extension_fileapi_apitest.cc',
        'browser/extensions/extension_font_settings_apitest.cc',
        'browser/extensions/extension_function_test_utils.cc',
        'browser/extensions/extension_function_test_utils.h',
        'browser/extensions/extension_geolocation_apitest.cc',
        'browser/extensions/extension_get_views_apitest.cc',
        'browser/extensions/extension_i18n_apitest.cc',
        'browser/extensions/extension_icon_source_apitest.cc',
        'browser/extensions/extension_idle_apitest.cc',
        'browser/extensions/extension_incognito_apitest.cc',
        'browser/extensions/extension_info_private_apitest_chromeos.cc',
        'browser/extensions/extension_input_apitest.cc',
        'browser/extensions/extension_input_ime_apitest_chromeos.cc',
        'browser/extensions/extension_input_method_apitest_chromeos.cc',
        'browser/extensions/extension_input_ui_apitest.cc',
        'browser/extensions/extension_install_ui_browsertest.cc',
        'browser/extensions/extension_javascript_url_apitest.cc',
        'browser/extensions/extension_keybinding_apitest.cc',
        'browser/extensions/extension_managed_mode_apitest.cc',
        'browser/extensions/extension_management_api_browsertest.cc',
        'browser/extensions/extension_management_apitest.cc',
        'browser/extensions/extension_management_browsertest.cc',
        'browser/extensions/extension_messages_apitest.cc',
        'browser/extensions/extension_metrics_apitest.cc',
        'browser/extensions/extension_module_apitest.cc',
        'browser/extensions/extension_override_apitest.cc',
        'browser/extensions/extension_page_capture_apitest.cc',
        'browser/extensions/extension_preference_apitest.cc',
        'browser/extensions/extension_processes_apitest.cc',
        'browser/extensions/extension_record_api_test.cc',
        'browser/extensions/extension_resource_request_policy_apitest.cc',
        'browser/extensions/extension_startup_browsertest.cc',
        'browser/extensions/extension_storage_apitest.cc',
        'browser/extensions/extension_tabs_apitest.cc',
        'browser/extensions/extension_tabs_test.cc',
        'browser/extensions/extension_test_message_listener.cc',
        'browser/extensions/extension_test_message_listener.h',
        'browser/extensions/extension_toolbar_model_browsertest.cc',
        'browser/extensions/extension_url_rewrite_browsertest.cc',
        'browser/extensions/extension_web_socket_proxy_private_apitest.cc',
        'browser/extensions/extension_websocket_apitest.cc',
        'browser/extensions/extension_webstore_private_apitest.cc',
        'browser/extensions/isolated_app_browsertest.cc',
        'browser/extensions/lazy_background_page_apitest.cc',
        'browser/extensions/mutation_observers_apitest.cc',
        'browser/extensions/notifications_apitest.cc',
        'browser/extensions/platform_app_browsertest.cc',
        'browser/extensions/process_management_browsertest.cc',
        'browser/extensions/settings/settings_apitest.cc',
        'browser/extensions/stubs_apitest.cc',
        'browser/extensions/system/system_apitest.cc',
        'browser/extensions/webstore_inline_install_browsertest.cc',
        'browser/extensions/window_open_apitest.cc',
        'browser/external_extension_browsertest.cc',
        'browser/fast_shutdown_browsertest.cc',
        'browser/first_run/first_run_browsertest.cc',
        'browser/first_run/try_chrome_dialog_view_browsertest.cc',
        'browser/geolocation/access_token_store_browsertest.cc',
        'browser/geolocation/geolocation_browsertest.cc',
        'browser/history/history_browsertest.cc',
        'browser/history/history_extension_apitest.cc',
        'browser/history/multipart_browsertest.cc',
        'browser/history/redirect_browsertest.cc',
        'browser/history/top_sites_extension_test.cc',
        'browser/iframe_browsertest.cc',
        'browser/infobars/infobars_browsertest.cc',
        'browser/infobars/infobar_extension_apitest.cc',
        'browser/importer/toolbar_importer_utils_browsertest.cc',
        'browser/loadtimes_extension_bindings_browsertest.cc',
        'browser/locale_tests_browsertest.cc',
        'browser/logging_chrome_browsertest.cc',
        'browser/metrics/metrics_service_browsertest.cc',
        'browser/net/cookie_policy_browsertest.cc',
        'browser/net/ftp_browsertest.cc',
        'browser/net/load_timing_observer_browsertest.cc',
        'browser/notifications/desktop_notifications_unittest.cc',
        'browser/notifications/desktop_notifications_unittest.h',
        'browser/notifications/notification_browsertest.cc',
        'browser/policy/device_management_service_browsertest.cc',
        'browser/popup_blocker_browsertest.cc',
        'browser/prefs/pref_service_browsertest.cc',
        'browser/prerender/prefetch_browsertest.cc',
        'browser/prerender/prerender_browsertest.cc',
        'browser/printing/cloud_print/test/cloud_print_proxy_process_browsertest.cc',
        'browser/printing/printing_layout_browsertest.cc',
        'browser/printing/print_preview_tab_controller_browsertest.cc',
        'browser/process_singleton_browsertest.cc',
        'browser/profiles/profile_browsertest.cc',
        'browser/profiles/profile_manager_browsertest.cc',
        'browser/protector/default_search_provider_change_browsertest.cc',
        'browser/protector/protector_service_browsertest.cc',
        'browser/repost_form_warning_browsertest.cc',
        'browser/rlz/rlz_extension_apitest.cc',
        'browser/referrer_policy_browsertest.cc',
        'browser/renderer_host/render_process_host_chrome_browsertest.cc',
        'browser/renderer_host/web_cache_manager_browsertest.cc',
        'browser/safe_browsing/safe_browsing_blocking_page_test.cc',
        'browser/safe_browsing/safe_browsing_service_browsertest.cc',
        'browser/service/service_process_control_browsertest.cc',
        'browser/sessions/session_restore_browsertest.cc',
        'browser/sessions/tab_restore_service_browsertest.cc',
        'browser/speech/extension_api/tts_extension_apitest.cc',
        'browser/speech/speech_input_extension_apitest.cc',
        'browser/speech/speech_recognition_bubble_browsertest.cc',
        'browser/spellchecker/spellcheck_host_browsertest.cc',
        'browser/ssl/ssl_browser_tests.cc',
        'browser/tab_contents/render_view_context_menu_browsertest.cc',
        'browser/tab_contents/render_view_context_menu_browsertest_util.cc',
        'browser/tab_contents/render_view_context_menu_browsertest_util.h',
        'browser/tab_contents/spelling_menu_observer_browsertest.cc',
        'browser/tab_contents/view_source_browsertest.cc',
        'browser/tab_render_watcher_browsertest.cc',
        'browser/tab_restore_browsertest.cc',
        'browser/task_manager/task_manager_browsertest.cc',
        'browser/task_manager/task_manager_browsertest_util.cc',
        'browser/task_manager/task_manager_browsertest_util.h',
        'browser/task_manager/task_manager_notification_browsertest.cc',
        'browser/translate/translate_manager_browsertest.cc',
        'browser/ui/browser_browsertest.cc',
        'browser/ui/browser_close_browsertest.cc',
        'browser/ui/browser_navigator_browsertest.cc',
        'browser/ui/browser_navigator_browsertest.h',
        'browser/ui/browser_navigator_browsertest_chromeos.cc',
        'browser/ui/cocoa/content_settings/content_setting_bubble_cocoa_unittest.mm',
        'browser/ui/cocoa/view_id_util_browsertest.mm',
        'browser/ui/cocoa/applescript/browsercrapplication+applescript_test.mm',
        'browser/ui/cocoa/applescript/window_applescript_test.mm',
        'browser/ui/cocoa/web_intent_sheet_controller_browsertest.mm',
        'browser/ui/find_bar/find_bar_host_browsertest.cc',
        'browser/ui/global_error_service_browsertest.cc',
        'browser/ui/gtk/one_click_signin_bubble_gtk_browsertest.cc',
        'browser/ui/gtk/view_id_util_browsertest.cc',
        'browser/ui/intents/web_intent_picker_controller_browsertest.cc',
        'browser/ui/login/login_prompt_browsertest.cc',
        'browser/ui/panels/panel_app_browsertest.cc',
        'browser/ui/panels/panel_browser_view_browsertest.cc',
        'browser/ui/prefs/prefs_tab_helper_browsertest.cc',
        'browser/ui/startup/startup_browser_creator_browsertest.cc',
        'browser/ui/tab_modal_confirm_dialog_browsertest_mac.mm',
        'browser/ui/tab_modal_confirm_dialog_browsertest.cc',
        'browser/ui/tab_modal_confirm_dialog_browsertest.h',
        'browser/ui/views/ash/browser_non_client_frame_view_ash_browsertest.cc',
        'browser/ui/views/ash/caps_lock_handler_browsertest.cc',
        'browser/ui/views/ash/launcher/launcher_favicon_loader_browsertest.cc',
        'browser/ui/views/browser_actions_container_browsertest.cc',
        'browser/ui/views/constrained_window_views_browsertest.cc',
        'browser/ui/views/frame/app_non_client_frame_view_aura_browsertest.cc',
        'browser/ui/views/frame/browser_view_browsertest.cc',
        'browser/ui/views/select_file_dialog_extension_browsertest.cc',
        'browser/ui/views/sync/one_click_signin_bubble_view_browsertest.cc',
        'browser/ui/views/web_dialog_view_browsertest.cc',
        'browser/ui/webui/chrome_url_data_manager_browsertest.cc',
        'browser/ui/webui/help/help_browsertest.js',
        'browser/ui/webui/ntp/most_visited_browsertest.cc',
        'browser/ui/webui/test_chrome_web_ui_controller_factory_browsertest.cc',
        'browser/ui/webui/bidi_checker_web_ui_test.cc',
        'browser/ui/webui/bidi_checker_web_ui_test.h',
        'browser/ui/webui/bookmarks_ui_browsertest.cc',
        'browser/ui/webui/extensions/extension_settings_browsertest.js',
        'browser/ui/webui/inspect_ui_browsertest.cc',
        'browser/ui/webui/net_internals/net_internals_ui_browsertest.cc',
        'browser/ui/webui/net_internals/net_internals_ui_browsertest.h',
        'browser/ui/webui/ntp/new_tab_ui_browsertest.cc',
        'browser/ui/webui/options2/autofill_options2_browsertest.js',
        'browser/ui/webui/options2/browser_options2_browsertest.js',
        'browser/ui/webui/options2/certificate_manager2_browsertest.js',
        'browser/ui/webui/options2/chromeos/guest_mode_options_ui2_browsertest.cc',
        'browser/ui/webui/options2/content_options2_browsertest.js',
        'browser/ui/webui/options2/content_settings_exception_area2_browsertest.js',
        'browser/ui/webui/options2/cookies_view2_browsertest.js',
        'browser/ui/webui/options2/font_settings2_browsertest.js',
        'browser/ui/webui/options2/language_options2_browsertest.js',
        'browser/ui/webui/options2/options2_browsertest.js',
        'browser/ui/webui/options2/options_ui2_browsertest.cc',
        'browser/ui/webui/options2/options_ui2_browsertest.h',
        'browser/ui/webui/options2/password_manager2_browsertest.js',
        'browser/ui/webui/options2/search_engine_manager2_browsertest.js',
        'browser/ui/webui/print_preview/print_preview_ui_browsertest.cc',
        'browser/ui/webui/sync_setup_browsertest.js',
        'browser/ui/webui/test_web_dialog_delegate.cc',
        'browser/ui/webui/test_web_dialog_delegate.h',
        'browser/ui/webui/web_dialog_controller_browsertest.cc',
        'browser/ui/webui/web_ui_browsertest.cc',
        'browser/ui/webui/web_ui_browsertest.h',
        'browser/ui/webui/web_ui_test_handler.cc',
        'browser/ui/webui/web_ui_test_handler.h',
        'browser/unload_browsertest.cc',
        'common/mac/mock_launchd.cc',
        'common/mac/mock_launchd.h',
        'common/time_format_browsertest.cc',
        'renderer/autofill/autofill_browsertest.cc',
        'renderer/autofill/form_autocomplete_browsertest.cc',
        'renderer/autofill/form_autofill_browsertest.cc',
        'renderer/autofill/password_autofill_manager_browsertest.cc',
        'renderer/autofill/password_generation_manager_browsertest.cc',
        'renderer/automation/automation_renderer_helper_browsertest.cc',
        'renderer/content_settings_observer_browsertest.cc',
        'renderer/page_click_tracker_browsertest.cc',
        'renderer/print_web_view_helper_browsertest.cc',
        'renderer/safe_browsing/malware_dom_details_browsertest.cc',
        'renderer/safe_browsing/phishing_classifier_browsertest.cc',
        'renderer/safe_browsing/phishing_classifier_delegate_browsertest.cc',
        'renderer/safe_browsing/phishing_dom_feature_extractor_browsertest.cc',
        'renderer/safe_browsing/phishing_thumbnailer_browsertest.cc',
        'renderer/translate_helper_browsertest.cc',
        'test/automation/dom_automation_browsertest.cc',
        'test/base/layout_test_http_server.cc',
        'test/base/layout_test_http_server.h',
        'test/base/in_process_browser_test_browsertest.cc',
        'test/base/chrome_render_view_test.cc',
        'test/base/chrome_render_view_test.h',
        'test/base/chrome_test_launcher.cc',
        'test/data/webui/assertions.js',
        'test/data/webui/async_gen.cc',
        'test/data/webui/async_gen.h',
        'test/data/webui/async_gen.js',
        'test/data/webui/certificate_viewer_dialog_test.js',
        'test/data/webui/certificate_viewer_ui_test-inl.h',
        'test/data/webui/chrome_send_browsertest.cc',
        'test/data/webui/chrome_send_browsertest.h',
        'test/data/webui/chrome_send_browsertest.js',
        'test/data/webui/mock4js_browsertest.js',
        'test/data/webui/net_internals/dns_view.js',
        'test/data/webui/net_internals/hsts_view.js',
        'test/data/webui/net_internals/http_pipeline_view.js',
        'test/data/webui/net_internals/log_util.js',
        'test/data/webui/net_internals/log_view_painter.js',
        'test/data/webui/net_internals/main.js',
        'test/data/webui/net_internals/net_internals_test.js',
        'test/data/webui/net_internals/prerender_view.js',
        'test/data/webui/net_internals/test_view.js',
        'test/data/webui/net_internals/timeline_view.js',
        'test/data/webui/ntp4.js',
        'test/data/webui/ntp4_browsertest.cc',
        'test/data/webui/ntp4_browsertest.h',
        'test/data/webui/print_preview.cc',
        'test/data/webui/print_preview.h',
        'test/data/webui/print_preview.js',
        'test/data/webui/suidsandbox_browsertest.js',
        'test/gpu/gpu_feature_browsertest.cc',
        'test/security_tests/sandbox_browsertest.cc',
        'test/ui/ppapi_uitest.cc',
        # TODO(craig): Rename this and run from base_unittests when the test
        # is safe to run there. See http://crbug.com/78722 for details.
        '../base/files/file_path_watcher_browsertest.cc',
        '../content/app/startup_helper_win.cc',
        '../content/browser/accessibility/dump_accessibility_tree_browsertest.cc',
        '../content/browser/accessibility/dump_accessibility_tree_helper.cc',
        '../content/browser/accessibility/dump_accessibility_tree_helper_mac.mm',
        '../content/browser/accessibility/dump_accessibility_tree_helper_win.cc',
        '../content/browser/accessibility/renderer_accessibility_browsertest.cc',
        '../content/browser/appcache/appcache_browsertest.cc',
        '../content/browser/audio_browsertest.cc',
        '../content/browser/child_process_security_policy_browsertest.cc',
        '../content/browser/device_orientation/device_orientation_browsertest.cc',
        '../content/browser/dom_storage/dom_storage_browsertest.cc',
        '../content/browser/download/mhtml_generation_browsertest.cc',
        '../content/browser/fileapi/file_system_browsertest.cc',
        '../content/browser/in_process_webkit/indexed_db_browsertest.cc',
        '../content/browser/in_process_webkit/indexed_db_layout_browsertest.cc',
        '../content/browser/indexed_db/idbbindingutilities_browsertest.cc',
        '../content/browser/media_browsertest.cc',
        '../content/browser/plugin_browsertest.cc',
        '../content/browser/plugin_data_remover_impl_browsertest.cc',
        '../content/browser/plugin_service_impl_browsertest.cc',
        '../content/browser/renderer_host/render_process_host_browsertest.cc',
        '../content/browser/renderer_host/render_view_host_browsertest.cc',
        '../content/browser/renderer_host/render_view_host_manager_browsertest.cc',
        '../content/browser/renderer_host/resource_dispatcher_host_browsertest.cc',
        '../content/browser/session_history_browsertest.cc',
        '../content/browser/speech/speech_recognition_browsertest.cc',
        '../content/browser/webkit_browsertest.cc',
        '../content/browser/worker_host/test/worker_browsertest.cc',
        '../content/renderer/mouse_lock_dispatcher_browsertest.cc',
        '../content/renderer/render_view_browsertest.cc',
        '../content/renderer/render_view_browsertest_mac.mm',
        '../content/test/gpu/test_switches.cc',
        '../content/test/gpu/test_switches.h',
        '../content/test/layout_browsertest.cc',
        '../content/test/layout_browsertest.h',
        '../content/test/render_widget_browsertest.cc',
        '../content/test/render_widget_browsertest.h',
        '../content/test/test_launcher.cc',
        '../content/test/test_launcher.h',
      ],
      'rules': [
        {
          'rule_name': 'js2webui',
          'extension': 'js',
          'msvs_external_rule': 1,
          'inputs': [
            '<(gypv8sh)',
            '<(PRODUCT_DIR)/v8_shell<(EXECUTABLE_SUFFIX)',
            '<(mock_js)',
            '<(test_api_js)',
            '<(js2gtest)',
          ],
          'outputs': [
            '<(INTERMEDIATE_DIR)/chrome/<(RULE_INPUT_DIRNAME)/<(RULE_INPUT_ROOT)-gen.cc',
            '<(PRODUCT_DIR)/test_data/chrome/<(RULE_INPUT_DIRNAME)/<(RULE_INPUT_ROOT).js',
          ],
          'process_outputs_as_sources': 1,
          'action': [
            'python',
            '<@(_inputs)',
            'webui',
            '<(RULE_INPUT_PATH)',
            'chrome/<(RULE_INPUT_DIRNAME)/<(RULE_INPUT_ROOT).js',
            '<@(_outputs)',
          ],
        },
      ],
      'conditions': [
        ['enable_one_click_signin==0', {
          'sources!': [
            'browser/ui/gtk/one_click_signin_bubble_gtk_browsertest.cc',
            'browser/ui/views/sync/one_click_signin_bubble_view_browsertest.cc',
          ]
        }],
        ['disable_nacl==0', {
          'sources':[
            'browser/extensions/extension_nacl_browsertest.cc',
          ],
          'conditions': [
            ['disable_nacl_untrusted==0', {
              'dependencies': [
              '../ppapi/ppapi_untrusted.gyp:ppapi_nacl_tests',
              ],
            }],
            ['OS=="win" or OS=="linux"', {
              'sources': [
                'browser/nacl_host/test/nacl_gdb_browsertest.cc',
              ],
              'dependencies': [
                'browser/nacl_host/test/mock_nacl_gdb.gyp:mock_nacl_gdb',
              ],
            }],
          ],
        }],
        ['chromeos==0', {
          'sources/': [
            ['exclude', '^browser/chromeos'],
            ['exclude', '^browser/ui/webui/options2/chromeos/'],
          ],
          'sources!': [
            'browser/extensions/api/terminal/terminal_private_apitest.cc',
            'test/data/webui/certificate_viewer_dialog_test.js',
            'test/data/webui/certificate_viewer_ui_test-inl.h',
          ],
        }, { #else: OS == "chromeos"
          'sources/': [
            # Protector is disabled on CrOS.
            ['exclude', '^browser/protector'],
          ],
          'sources!': [
            'browser/notifications/desktop_notifications_unittest.cc',
            'browser/printing/cloud_print/test/cloud_print_proxy_process_browsertest.cc',
            'browser/service/service_process_control_browsertest.cc',
            # chromeos does not use cross-platform panels
            'browser/ui/panels/panel_app_browsertest.cc',
            'browser/ui/panels/panel_browser_view_browsertest.cc',
          ],
        }],
        ['use_virtual_keyboard==1', {
          'sources': [
            'browser/ui/virtual_keyboard/virtual_keyboard_manager_browsertest.cc',
          ],
        }],
        ['file_manager_extension==0', {
          'sources!': [
            'browser/ui/views/select_file_dialog_extension_browsertest.cc',
          ],
        }],
        ['chromeos==0 or use_virtual_keyboard==0', {
          'sources!': [
            'browser/extensions/extension_input_ui_apitest.cc',
          ],
        }],
        ['configuration_policy==0', {
          'sources/': [
            ['exclude', '^browser/policy/'],
          ],
        }],
        ['input_speech==0', {
          'sources/': [
            ['exclude', '^browser/speech/'],
            ['exclude', '^../content/browser/speech/'],
          ],
        }],
        ['notifications==0', {
          'sources!': [
            'browser/extensions/notifications_apitest.cc',
          ],
        }],
        ['safe_browsing==1', {
          'defines': [
            'ENABLE_SAFE_BROWSING',
          ],
        }, {  # safe_browsing == 0
          'sources/': [
            ['exclude', '^browser/safe_browsing/'],
            ['exclude', '^renderer/safe_browsing/'],
          ],
        }],
        ['internal_pdf', {
          'sources': [
            'browser/ui/pdf/pdf_browsertest.cc',
          ],
        }],
        ['OS!="linux" or toolkit_views==1', {
          'sources!': [
            'browser/extensions/browser_action_test_util_gtk.cc',
            'browser/ui/gtk/view_id_util_browsertest.cc',
          ],
        }],
        ['OS=="linux"', {
          'sources!': [
            '../content/browser/accessibility/dump_accessibility_tree_browsertest.cc',
            '../content/browser/accessibility/dump_accessibility_tree_helper.cc',
          ],
        }],
        ['OS!="win" and OS!="mac"', {
          'sources!': [
            'browser/rlz/rlz_extension_apitest.cc',
          ],
        }],
        ['OS=="win"', {
          'sources': [
            '<(SHARED_INTERMEDIATE_DIR)/chrome/browser_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/common_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/extensions_api_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/renderer_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/theme_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/theme_resources_standard.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome_version/other_version.rc',
            '<(SHARED_INTERMEDIATE_DIR)/content/content_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/net/net_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/ui/ui_resources/ui_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/ui/ui_resources_standard/ui_resources_standard.rc',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/webkit_chromium_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/webkit_resources.rc',
          ],
          'include_dirs': [
            '<(DEPTH)/third_party/wtl/include',
          ],
          'dependencies': [
            'chrome_version_resources',
            'installer_util_strings',
            'security_tests',  # run time dependency
            '../third_party/iaccessible2/iaccessible2.gyp:iaccessible2',
            '../sandbox/sandbox.gyp:sandbox',
          ],
          'conditions': [
            ['win_use_allocator_shim==1', {
              'dependencies': [
                '<(allocator_target)',
              ],
            }],
          ],
          'configurations': {
            'Debug_Base': {
              'msvs_settings': {
                'VCLinkerTool': {
                  'LinkIncremental': '<(msvs_debug_link_nonincremental)',
                },
              },
            },
          }
        }, { # else: OS != "win"
          'sources!': [
            'app/chrome_command_ids.h',
            'app/chrome_dll.rc',
            'app/chrome_dll_resource.h',
            'app/chrome_version.rc.version',
            # TODO(port): http://crbug.com/45770
            'browser/printing/printing_layout_browsertest.cc',
            'browser/ui/views/constrained_window_views_browsertest.cc',
          ],
        }],
        ['toolkit_uses_gtk == 1', {
          'dependencies': [
            '../build/linux/system.gyp:gtk',
            '../tools/xdisplaycheck/xdisplaycheck.gyp:xdisplaycheck',
          ],
          'sources': [
            # TODO(estade): port to win/mac.
            'browser/ui/webui/constrained_web_dialog_ui_browsertest.cc',
          ],
        }],
        ['toolkit_uses_gtk == 1 or chromeos==1 or (OS=="linux" and use_aura==1)', {
          'dependencies': [
            '../build/linux/system.gyp:ssl',
          ],
        }],
        ['toolkit_uses_gtk == 1 and toolkit_views == 0', {
          'sources': [
            # BubbleGtk is used only on Linux/GTK.
            'browser/ui/gtk/bubble/bubble_gtk_browsertest.cc',
            'browser/ui/gtk/confirm_bubble_view_browsertest.cc',
          ],
        }],
        ['OS=="mac"', {
          'include_dirs': [
            '../third_party/GTM',
          ],
          # TODO(mark): We really want this for all non-static library
          # targets, but when we tried to pull it up to the common.gypi
          # level, it broke other things like the ui, startup, and
          # page_cycler tests. *shrug*
          'xcode_settings': {
            'OTHER_LDFLAGS': [
              '-Wl,-ObjC',
            ],
          },
          # Other platforms only need
          # chrome_resources.gyp:{packed_extra_resources,packed_resources},
          # and can build this target standalone much faster.
          'dependencies': [
            'chrome'
          ],
          'sources': [
            'browser/spellchecker/spellcheck_message_filter_mac_browsertest.cc',
            '../content/renderer/external_popup_menu_browsertest.cc',
          ],
          'sources!': [
            # TODO(hbono): This test depends on hunspell and we cannot run it on
            # Mac, which does not use hunspell by default.
            'browser/spellchecker/spellcheck_host_browsertest.cc',
            # ProcessSingletonMac doesn't do anything.
            'browser/process_singleton_browsertest.cc',            
          ],
        }, { # else: OS != "mac"
          'sources!': [
            'browser/extensions/browser_action_test_util_mac.mm',
          ],
        }],
        ['os_posix == 0 or chromeos == 1', {
          'sources!': [
            'common/time_format_browsertest.cc',
          ],
        }],
        ['os_posix == 1 and OS != "mac"', {
          'conditions': [
            ['linux_use_tcmalloc==1', {
              'dependencies': [
                '../base/allocator/allocator.gyp:allocator',
              ],
            }],
          ],
        }],
        ['toolkit_views==1', {
          'dependencies': [
            '../ui/views/views.gyp:views',
          ],
          'sources!': [
            # TODO(estade): port to linux/views.
            'browser/ui/webui/constrained_web_dialog_ui_browsertest.cc',
          ],
        }, { # else: toolkit_views == 0
          'sources!': [
            'browser/extensions/browser_action_test_util_views.cc',
            'browser/extensions/extension_input_apitest.cc',
            'browser/tab_render_watcher_browsertest.cc',
            'browser/ui/panels/panel_browser_view_browsertest.cc',
            'browser/ui/views/browser_actions_container_browsertest.cc',
            'browser/ui/views/crypto_module_password_dialog_view_unittest.cc',
            'browser/ui/views/frame/browser_view_browsertest.cc',
            'browser/ui/views/sync/one_click_signin_bubble_view_browsertest.cc',
            'browser/ui/views/web_dialog_view_browsertest.cc',
          ],
        }],
        ['target_arch!="arm"', {
          'dependencies': [
            # build time dependency.
            '../v8/tools/gyp/v8.gyp:v8_shell#host',
            # run time dependency
            '../webkit/webkit.gyp:copy_npapi_test_plugin',
          ],
        }],
        ['component=="shared_library" and incremental_chrome_dll!=1', {
          # This is needed for tests that subclass
          # RendererWebKitPlatformSupportImpl, which subclasses stuff in
          # glue, which refers to symbols defined in these files.
          # Hopefully this can be resolved with http://crbug.com/98755.
          'sources': [
            '../content/common/socket_stream_dispatcher.cc',
          ]},
        ],
        ['use_aura==1', {
          'sources!': [
            '../content/browser/plugin_browsertest.cc',
            '../content/browser/accessibility/dump_accessibility_tree_browsertest.cc',
            '../content/browser/accessibility/dump_accessibility_tree_helper_win.cc',
            '../content/browser/accessibility/dump_accessibility_tree_helper.cc',
          ]
        }, { # else: use_aura == 0 
          'sources!': [
            'browser/ui/views/frame/app_non_client_frame_view_aura_browsertest.cc',
        ],
        }],
      ],  # conditions
    },  # target browser_tests
    {
      'target_name': 'browser_tests_run',
      'type': 'none',
      'dependencies': [
        'browser_tests',
      ],
      'includes': [
        'browser_tests.isolate',
      ],
      'actions': [
        {
          'action_name': 'isolate',
          'inputs': [
            '<@(isolate_dependency_tracked)',
          ],
          'outputs': [
            '<(PRODUCT_DIR)/browser_tests.results',
          ],
          'action': [
            'python',
            '../tools/isolate/isolate.py',
            '--mode', '<(test_isolation_mode)',
            '--outdir', '<(test_isolation_outdir)',
            '--variable', 'PRODUCT_DIR', '<(PRODUCT_DIR)',
            '--variable', 'OS', '<(OS)',
            '--result', '<@(_outputs)',
            'browser_tests.isolate',
          ],
        },
      ],
    },
    {
      # Executable that runs each perf browser test in a new process.
      'target_name': 'performance_browser_tests',
      'type': 'executable',
      'msvs_cygwin_shell': 0,
      'msvs_cygwin_dirs': ['<(DEPTH)/third_party/cygwin'],
      'dependencies': [
        'browser',
        '../sync/protocol/sync_proto.gyp:sync_proto',
        'chrome_resources.gyp:chrome_resources',
        'chrome_resources.gyp:chrome_strings',
        'chrome_resources.gyp:packed_extra_resources',
        'chrome_resources.gyp:packed_resources',
        'renderer',
        'test_support_common',
        '../base/base.gyp:base',
        '../base/base.gyp:base_i18n',
        '../base/base.gyp:test_support_base',
        '../net/net.gyp:net',
        '../net/net.gyp:net_test_support',
        '../skia/skia.gyp:skia',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        '../third_party/cld/cld.gyp:cld',
        '../third_party/icu/icu.gyp:icui18n',
        '../third_party/icu/icu.gyp:icuuc',
        '../third_party/leveldatabase/leveldatabase.gyp:leveldatabase',
        '../v8/tools/gyp/v8.gyp:v8',
        '../webkit/webkit.gyp:test_shell_test_support',
        # Runtime dependencies
        '../third_party/mesa/mesa.gyp:osmesa',
        '../webkit/webkit.gyp:pull_in_copy_TestNetscapePlugIn',
      ],
      'include_dirs': [
        '..',
      ],
      'defines': [
        'HAS_OUT_OF_PROC_TEST_RUNNER',
        'BROWSER_TESTS_HEADER_OVERRIDE="chrome/test/base/in_process_browser_test.h"',
      ],
      'sources': [
        'app/breakpad_mac_stubs.mm',
        'app/chrome_command_ids.h',
        'app/chrome_dll.rc',
        'app/chrome_dll_resource.h',
        'app/chrome_version.rc.version',
        'test/base/chrome_render_view_test.cc',
        'test/base/chrome_render_view_test.h',
        'test/base/chrome_test_launcher.cc',
        'test/perf/browser_perf_test.cc',
        'test/perf/browser_perf_test.h',
        'test/perf/rendering/latency_tests.cc',
        'test/perf/rendering/throughput_tests.cc',
        '../content/app/startup_helper_win.cc',
        '../content/test/test_launcher.cc',
        '../content/test/test_launcher.h',
      ],
      'rules': [
        {
          'rule_name': 'js2webui',
          'extension': 'js',
          'msvs_external_rule': 1,
          'inputs': [
            '<(gypv8sh)',
            '<(PRODUCT_DIR)/v8_shell<(EXECUTABLE_SUFFIX)',
            '<(mock_js)',
            '<(test_api_js)',
            '<(js2gtest)',
          ],
          'outputs': [
            '<(INTERMEDIATE_DIR)/chrome/<(RULE_INPUT_DIRNAME)/<(RULE_INPUT_ROOT)-gen.cc',
            '<(PRODUCT_DIR)/test_data/chrome/<(RULE_INPUT_DIRNAME)/<(RULE_INPUT_ROOT).js',
          ],
          'process_outputs_as_sources': 1,
          'action': [
            'python',
            '<@(_inputs)',
            'webui',
            '<(RULE_INPUT_PATH)',
            'chrome/<(RULE_INPUT_DIRNAME)/<(RULE_INPUT_ROOT).js',
            '<@(_outputs)',
          ],
        },
      ],
      'conditions': [
        ['OS!="linux" or toolkit_views==1', {
          'sources!': [
            'browser/extensions/browser_action_test_util_gtk.cc',
          ],
        }],
        ['OS=="win"', {
          'sources': [
            '<(SHARED_INTERMEDIATE_DIR)/chrome/browser_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/common_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/extensions_api_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/renderer_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/theme_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/theme_resources_standard.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome_version/other_version.rc',
            '<(SHARED_INTERMEDIATE_DIR)/content/content_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/net/net_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/ui/ui_resources/ui_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/ui/ui_resources_standard/ui_resources_standard.rc',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/webkit_chromium_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/webkit_resources.rc',
          ],
          'include_dirs': [
            '<(DEPTH)/third_party/wtl/include',
          ],
          'dependencies': [
            'chrome_version_resources',
            'installer_util_strings',
            '../sandbox/sandbox.gyp:sandbox',
          ],
          'conditions': [
            ['win_use_allocator_shim==1', {
              'dependencies': [
                '<(allocator_target)',
              ],
            }],
          ],
          'configurations': {
            'Debug_Base': {
              'msvs_settings': {
                'VCLinkerTool': {
                  'LinkIncremental': '<(msvs_debug_link_nonincremental)',
                },
              },
            },
          }
        }, { # else: OS != "win"
          'sources!': [
            'app/chrome_command_ids.h',
            'app/chrome_dll.rc',
            'app/chrome_dll_resource.h',
            'app/chrome_version.rc.version',
          ],
        }],
        ['toolkit_uses_gtk == 1', {
          'dependencies': [
            '../build/linux/system.gyp:gtk',
            '../tools/xdisplaycheck/xdisplaycheck.gyp:xdisplaycheck',
          ],
        }],
        ['toolkit_uses_gtk == 1 or chromeos==1 or (OS=="linux" and use_aura==1)', {
          'dependencies': [
            '../build/linux/system.gyp:ssl',
          ],
        }],
        ['OS=="mac"', {
          'include_dirs': [
            '../third_party/GTM',
          ],
          # TODO(mark): We really want this for all non-static library
          # targets, but when we tried to pull it up to the common.gypi
          # level, it broke other things like the ui, startup, and
          # page_cycler tests. *shrug*
          'xcode_settings': {
            'OTHER_LDFLAGS': [
              '-Wl,-ObjC',
            ],
          },
          # Other platforms only need
          # chrome_resources.gyp:{packed_extra_resources,packed_resources},
          # and can build this target standalone much faster.
          'dependencies': [
            'chrome'
          ],
        }, { # else: OS != "mac"
          'sources!': [
            'browser/extensions/browser_action_test_util_mac.mm',
          ],
        }],
        ['os_posix == 1 and OS != "mac"', {
          'conditions': [
            ['linux_use_tcmalloc==1', {
              'dependencies': [
                '../base/allocator/allocator.gyp:allocator',
              ],
            }],
          ],
        }],
      ],  # conditions
    },  # target performance_browser_tests
    {
      # Executable that runs safebrowsing test in a new process.
      'target_name': 'safe_browsing_tests',
      'type': 'executable',
      'dependencies': [
        'chrome',
        'test_support_common',
        '../base/base.gyp:base',
        '../net/net.gyp:net_test_support',
        '../skia/skia.gyp:skia',
        '../testing/gtest.gyp:gtest',
        # This is the safebrowsing test server.
        '../third_party/safe_browsing/safe_browsing.gyp:safe_browsing',
        '../ui/ui.gyp:ui_resources',
        '../ui/ui.gyp:ui_resources_standard',
      ],
      'include_dirs': [
        '..',
      ],
      'defines': [
        'HAS_OUT_OF_PROC_TEST_RUNNER',
        'BROWSER_TESTS_HEADER_OVERRIDE="chrome/test/base/in_process_browser_test.h"',
      ],
      'sources': [
        'app/chrome_dll.rc',
        'browser/safe_browsing/safe_browsing_test.cc',
        'test/base/chrome_test_launcher.cc',
        '../content/app/startup_helper_win.cc',
        '../content/test/test_launcher.cc',
        '../content/test/test_launcher.h',
      ],
      'conditions': [
        ['safe_browsing==0', {
          'sources!': [
            'browser/safe_browsing/safe_browsing_test.cc',
          ],
        }],
        ['OS=="win"', {
          'dependencies': [
            'chrome_version_resources',
            'installer_util_strings',
            '../sandbox/sandbox.gyp:sandbox',
          ],
          'sources': [
            '<(SHARED_INTERMEDIATE_DIR)/chrome/browser_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/common_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/extensions_api_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/renderer_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/theme_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/theme_resources_standard.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome_version/other_version.rc',
            '<(SHARED_INTERMEDIATE_DIR)/content/content_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/net/net_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/ui/ui_resources/ui_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/ui/ui_resources_standard/ui_resources_standard.rc',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/webkit_chromium_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/webkit_resources.rc',
          ],
          'configurations': {
            'Debug_Base': {
              'msvs_settings': {
                'VCLinkerTool': {
                  'LinkIncremental': '<(msvs_large_module_debug_link_mode)',
                },
              },
            },
          },
        }],
        ['OS=="mac"', {
          # These flags are needed to run the test on Mac.
          # Search for comments about "xcode_settings" elsewhere in this file.
          'xcode_settings': {'OTHER_LDFLAGS': ['-Wl,-ObjC']},
        }],
      ],
    },  # target safe_browsing_tests
    {
      # To run the tests from page_load_test.cc on Linux, we need to:
      #
      #   a) Build with Breakpad (GYP_DEFINES="linux_chromium_breakpad=1")
      #   b) Run with CHROME_HEADLESS=1 to generate crash dumps.
      #   c) Strip the binary if it's a debug build. (binary may be over 2GB)
      'target_name': 'reliability_tests',
      'type': 'executable',
      'dependencies': [
        'browser',
        'chrome',
        'chrome_resources.gyp:theme_resources',
        'test_support_common',
        'test_support_ui',
        '../skia/skia.gyp:skia',
        '../testing/gtest.gyp:gtest',
        '../third_party/WebKit/Source/WebKit/chromium/WebKit.gyp:webkit',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'test/reliability/page_load_test.cc',
      ],
      'conditions': [
        ['OS=="win" and buildtype=="Official"', {
          'configurations': {
            'Release': {
              'msvs_settings': {
                'VCCLCompilerTool': {
                  'WholeProgramOptimization': 'false',
                },
              },
            },
          },
        },],
        ['OS=="win" and win_use_allocator_shim==1', {
          'dependencies': [
            '<(allocator_target)',
          ],
          'configurations': {
            'Debug': {
              'msvs_settings': {
                'VCLinkerTool': {
                  'LinkIncremental': '<(msvs_large_module_debug_link_mode)',
                },
              },
            },
          },
        },],
        ['toolkit_uses_gtk == 1', {
          'dependencies': [
            '../build/linux/system.gyp:gtk',
          ],
        },],
      ],
    },
    {
      'target_name': 'performance_ui_tests',
      'type': 'executable',
      'dependencies': [
        'chrome',
        'chrome_resources.gyp:chrome_resources',
        'chrome_resources.gyp:chrome_strings',
        'debugger',
        'test_support_common',
        'test_support_ui',
        '../base/base.gyp:base',
        '../skia/skia.gyp:skia',
        '../testing/gtest.gyp:gtest',
      ],
      'sources': [
        # TODO(darin): Move other UIPerfTests here.
        'test/perf/dom_checker_uitest.cc',
        'test/perf/dromaeo_benchmark_uitest.cc',
        'test/perf/feature_startup_test.cc',
        'test/perf/frame_rate/frame_rate_tests.cc',
        'test/perf/kraken_benchmark_uitest.cc',
        'test/perf/memory_test.cc',
        'test/perf/page_cycler_test.cc',
        'test/perf/shutdown_test.cc',
        'test/perf/startup_test.cc',
        'test/perf/sunspider_uitest.cc',
        'test/perf/tab_switching_test.cc',
        'test/perf/url_fetch_test.cc',
        'test/perf/v8_benchmark_uitest.cc',
      ],
      'conditions': [
        ['OS=="win" and buildtype=="Official"', {
          'configurations': {
            'Release': {
              'msvs_settings': {
                'VCCLCompilerTool': {
                  'WholeProgramOptimization': 'false',
                },
              },
            },
          },
        }],
        ['OS=="win"', {
          'conditions': [
            ['win_use_allocator_shim==1', {
              'dependencies': [
                '<(allocator_target)',
              ],
            }],
          ],
          'configurations': {
            'Debug_Base': {
              'msvs_settings': {
                'VCLinkerTool': {
                  'LinkIncremental': '<(msvs_large_module_debug_link_mode)',
                },
              },
            },
          },
        }],
        ['OS=="mac"', {
          'sources': [
            'test/perf/mach_ports_test.cc',
          ],
        }],
        ['toolkit_uses_gtk == 1', {
          'dependencies': [
            '../build/linux/system.gyp:gtk',
            '../tools/xdisplaycheck/xdisplaycheck.gyp:xdisplaycheck',
          ],
        }],
        ['os_posix == 1 and OS != "mac"', {
          'conditions': [
            ['linux_use_tcmalloc==1', {
              'dependencies': [
                '../base/allocator/allocator.gyp:allocator',
              ],
            }],
          ],
        }],
        ['toolkit_views==1', {
          'dependencies': [
            '../ui/views/views.gyp:views',
          ],
        }],
      ],
      'actions': [
        {
          'variables': {
            'kraken_dir': 'test/data/third_party/kraken',
            'test_dir': 'test/data/third_party/kraken/tests/kraken-1.1',
            'output_dir': '<(PRODUCT_DIR)/test/data/third_party/kraken'
          },
          'action_name': 'kraken_make_hosted',
          'inputs': [
            '<(kraken_dir)/make-hosted.py',
            '<(kraken_dir)/resources/TEMPLATE.html',
            '<(kraken_dir)/resources/analyze-results.js',
            '<(kraken_dir)/resources/compare-results.js',
            '<(kraken_dir)/resources/driver-TEMPLATE.html',
            '<(kraken_dir)/resources/results-TEMPLATE.html',
            '<(kraken_dir)/resources/sunspider-standalone-compare.js',
            '<(kraken_dir)/resources/sunspider-standalone-driver.js',
            '<(test_dir)/LIST',
            '<(test_dir)/ai-astar-data.js',
            '<(test_dir)/ai-astar.js',
            '<(test_dir)/audio-beat-detection-data.js',
            '<(test_dir)/audio-beat-detection.js',
            '<(test_dir)/audio-dft-data.js',
            '<(test_dir)/audio-dft.js',
            '<(test_dir)/audio-fft-data.js',
            '<(test_dir)/audio-fft.js',
            '<(test_dir)/audio-oscillator-data.js',
            '<(test_dir)/audio-oscillator.js',
            '<(test_dir)/imaging-darkroom-data.js',
            '<(test_dir)/imaging-darkroom.js',
            '<(test_dir)/imaging-desaturate-data.js',
            '<(test_dir)/imaging-desaturate.js',
            '<(test_dir)/imaging-gaussian-blur-data.js',
            '<(test_dir)/imaging-gaussian-blur.js',
            '<(test_dir)/json-parse-financial-data.js',
            '<(test_dir)/json-parse-financial.js',
            '<(test_dir)/json-stringify-tinderbox-data.js',
            '<(test_dir)/json-stringify-tinderbox.js',
            '<(test_dir)/stanford-crypto-aes-data.js',
            '<(test_dir)/stanford-crypto-aes.js',
            '<(test_dir)/stanford-crypto-ccm-data.js',
            '<(test_dir)/stanford-crypto-ccm.js',
            '<(test_dir)/stanford-crypto-pbkdf2-data.js',
            '<(test_dir)/stanford-crypto-pbkdf2.js',
            '<(test_dir)/stanford-crypto-sha256-iterative-data.js',
            '<(test_dir)/stanford-crypto-sha256-iterative.js',
          ],
          'outputs': [
            '<(output_dir)/hosted/analyze-results.js',
            '<(output_dir)/hosted/compare-results.js',
            '<(output_dir)/hosted/json2.js',
            '<(output_dir)/hosted/kraken.css',
            '<(output_dir)/hosted/kraken-1.1/driver.html',
            '<(output_dir)/hosted/kraken-1.1/results.html',
            '<(output_dir)/hosted/kraken-1.1/test-contents.js',
            '<(output_dir)/hosted/kraken-1.1/test-prefix.js',
          ],
          'action': [
            'python',
            '<(kraken_dir)/make-hosted.py',
            '<(kraken_dir)',
            '<(output_dir)',
          ],
        }
      ],
      'copies': [
        {
          'destination': '<(PRODUCT_DIR)/test/data/third_party/kraken/hosted/kraken-1.1',
          'files': [
            'test/perf/kraken_benchmark_uitest.js',
          ],
        },
      ],
    },
    {
      'target_name': 'sync_integration_tests',
      'type': 'executable',
      'dependencies': [
        'browser',
        '../sync/protocol/sync_proto.gyp:sync_proto',
        'chrome',
        'chrome_resources.gyp:chrome_resources',
        'chrome_resources.gyp:chrome_strings',
        'common',
        'renderer',
        'test_support_common',
        '../net/net.gyp:net',
        '../net/net.gyp:net_test_support',
        '../printing/printing.gyp:printing',
        '../skia/skia.gyp:skia',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        '../third_party/icu/icu.gyp:icui18n',
        '../third_party/icu/icu.gyp:icuuc',
        '../third_party/leveldatabase/leveldatabase.gyp:leveldatabase',
        '../third_party/npapi/npapi.gyp:npapi',
        '../third_party/WebKit/Source/WebKit/chromium/WebKit.gyp:webkit',
      ],
      'include_dirs': [
        '..',
        '<(INTERMEDIATE_DIR)',
        '<(protoc_out_dir)',
      ],
      # TODO(phajdan.jr): Only temporary, to make transition easier.
      'defines': [
        'HAS_OUT_OF_PROC_TEST_RUNNER',
        'BROWSER_TESTS_HEADER_OVERRIDE="chrome/test/base/in_process_browser_test.h"',
      ],
      'sources': [
        'app/chrome_command_ids.h',
        'app/chrome_dll.rc',
        'app/chrome_dll_resource.h',
        'app/chrome_version.rc.version',
        'browser/password_manager/password_form_data.cc',
        'browser/sessions/session_backend.cc',
        'browser/sync/glue/session_model_associator.cc',
        'test/base/chrome_test_launcher.cc',
        'test/data/resource.rc',
        'browser/sync/test/integration/apps_helper.cc',
        'browser/sync/test/integration/apps_helper.h',
        'browser/sync/test/integration/autofill_helper.cc',
        'browser/sync/test/integration/autofill_helper.h',
        'browser/sync/test/integration/bookmarks_helper.cc',
        'browser/sync/test/integration/bookmarks_helper.h',
        'browser/sync/test/integration/enable_disable_test.cc',
        'browser/sync/test/integration/extension_settings_helper.cc',
        'browser/sync/test/integration/extension_settings_helper.h',
        'browser/sync/test/integration/extensions_helper.cc',
        'browser/sync/test/integration/extensions_helper.h',
        'browser/sync/test/integration/migration_errors_test.cc',
        'browser/sync/test/integration/multiple_client_bookmarks_sync_test.cc',
        'browser/sync/test/integration/multiple_client_passwords_sync_test.cc',
        'browser/sync/test/integration/multiple_client_preferences_sync_test.cc',
        'browser/sync/test/integration/multiple_client_sessions_sync_test.cc',
        'browser/sync/test/integration/multiple_client_typed_urls_sync_test.cc',
        'browser/sync/test/integration/passwords_helper.cc',
        'browser/sync/test/integration/passwords_helper.h',
        'browser/sync/test/integration/preferences_helper.cc',
        'browser/sync/test/integration/preferences_helper.h',
        'browser/sync/test/integration/search_engines_helper.cc',
        'browser/sync/test/integration/search_engines_helper.h',
        'browser/sync/test/integration/sessions_helper.cc',
        'browser/sync/test/integration/sessions_helper.h',
        'browser/sync/test/integration/single_client_apps_sync_test.cc',
        'browser/sync/test/integration/single_client_bookmarks_sync_test.cc',
        'browser/sync/test/integration/single_client_extensions_sync_test.cc',
        'browser/sync/test/integration/single_client_passwords_sync_test.cc',
        'browser/sync/test/integration/single_client_preferences_sync_test.cc',
        'browser/sync/test/integration/single_client_search_engines_sync_test.cc',
        'browser/sync/test/integration/single_client_sessions_sync_test.cc',
        'browser/sync/test/integration/single_client_themes_sync_test.cc',
        'browser/sync/test/integration/single_client_typed_urls_sync_test.cc',
        'browser/sync/test/integration/sync_app_helper.cc',
        'browser/sync/test/integration/sync_app_helper.h',
        'browser/sync/test/integration/sync_datatype_helper.cc',
        'browser/sync/test/integration/sync_datatype_helper.h',
        'browser/sync/test/integration/sync_errors_test.cc',
        'browser/sync/test/integration/sync_extension_helper.cc',
        'browser/sync/test/integration/sync_extension_helper.h',
        'browser/sync/test/integration/sync_test.cc',
        'browser/sync/test/integration/sync_test.h',
        'browser/sync/test/integration/themes_helper.cc',
        'browser/sync/test/integration/themes_helper.h',
        'browser/sync/test/integration/two_client_apps_sync_test.cc',
        'browser/sync/test/integration/two_client_autofill_sync_test.cc',
        'browser/sync/test/integration/two_client_bookmarks_sync_test.cc',
        'browser/sync/test/integration/two_client_extension_settings_and_app_settings_sync_test.cc',
        'browser/sync/test/integration/two_client_extensions_sync_test.cc',
        'browser/sync/test/integration/two_client_passwords_sync_test.cc',
        'browser/sync/test/integration/two_client_preferences_sync_test.cc',
        'browser/sync/test/integration/two_client_search_engines_sync_test.cc',
        'browser/sync/test/integration/two_client_sessions_sync_test.cc',
        'browser/sync/test/integration/two_client_themes_sync_test.cc',
        'browser/sync/test/integration/two_client_typed_urls_sync_test.cc',
        'browser/sync/test/integration/typed_urls_helper.cc',
        'browser/sync/test/integration/typed_urls_helper.h',
        '../content/app/startup_helper_win.cc',
        '../content/test/test_launcher.cc',
        '../content/test/test_launcher.h',
      ],
      'conditions': [
        ['toolkit_uses_gtk == 1', {
           'dependencies': [
             '../build/linux/system.gyp:gtk',
           ],
        }],
        ['toolkit_uses_gtk == 1 or chromeos==1 or (OS=="linux" and use_aura==1)', {
          'dependencies': [
            '../build/linux/system.gyp:ssl',
          ],
        }],
        ['OS=="mac"', {
          # The sync_integration_tests do not run on mac without this flag.
          # Search for comments about "xcode_settings" elsewhere in this file.
          'xcode_settings': {'OTHER_LDFLAGS': ['-Wl,-ObjC']},
        }],
        ['OS=="win"', {
          'sources': [
            '<(SHARED_INTERMEDIATE_DIR)/chrome/browser_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/common_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/extensions_api_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/theme_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/theme_resources_standard.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome_version/other_version.rc',
            '<(SHARED_INTERMEDIATE_DIR)/ui/ui_resources/ui_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/ui/ui_resources_standard/ui_resources_standard.rc',
          ],
          'include_dirs': [
            '<(DEPTH)/third_party/wtl/include',
          ],
          'dependencies': [
            'chrome_version_resources',
            'installer_util_strings',
            '../sandbox/sandbox.gyp:sandbox',
          ],
          'conditions': [
            ['win_use_allocator_shim==1', {
              'dependencies': [
                '<(allocator_target)',
              ],
            }],
          ],
          'configurations': {
            'Debug': {
              'msvs_settings': {
                'VCLinkerTool': {
                  'LinkIncremental': '<(msvs_debug_link_nonincremental)',
                },
              },
            },
          },
        }, { # else: OS != "win"
          'sources!': [
            'app/chrome_dll.rc',
            'app/chrome_version.rc.version',
            'test/data/resource.rc',
          ],
        }],
        ['toolkit_views==1', {
          'dependencies': [
            '../ui/views/views.gyp:views',
          ],
        }],
      ],
    },
    {
      'target_name': 'sync_performance_tests',
      'type': 'executable',
      'dependencies': [
        '../sync/protocol/sync_proto.gyp:sync_proto',
        'chrome',
        'test_support_common',
        '../skia/skia.gyp:skia',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
      ],
      'include_dirs': [
        '..',
        '<(INTERMEDIATE_DIR)',
        '<(protoc_out_dir)',
      ],
      'defines': [
        'HAS_OUT_OF_PROC_TEST_RUNNER',
        'BROWSER_TESTS_HEADER_OVERRIDE="chrome/test/base/in_process_browser_test.h"',
      ],
      'sources': [
        'app/chrome_command_ids.h',
        'app/chrome_dll.rc',
        'app/chrome_dll_resource.h',
        'app/chrome_version.rc.version',
        'browser/password_manager/password_form_data.cc',
        'browser/sessions/session_backend.cc',
        'browser/sync/glue/session_model_associator.cc',
        'browser/sync/test/integration/autofill_helper.cc',
        'browser/sync/test/integration/autofill_helper.h',
        'browser/sync/test/integration/bookmarks_helper.cc',
        'browser/sync/test/integration/bookmarks_helper.h',
        'browser/sync/test/integration/extensions_helper.cc',
        'browser/sync/test/integration/extensions_helper.h',
        'browser/sync/test/integration/passwords_helper.cc',
        'browser/sync/test/integration/passwords_helper.h',
        'browser/sync/test/integration/performance/autofill_sync_perf_test.cc',
        'browser/sync/test/integration/performance/bookmarks_sync_perf_test.cc',
        'browser/sync/test/integration/performance/extensions_sync_perf_test.cc',
        'browser/sync/test/integration/performance/sync_timing_helper.cc',
        'browser/sync/test/integration/performance/sync_timing_helper.h',
        'browser/sync/test/integration/performance/passwords_sync_perf_test.cc',
        'browser/sync/test/integration/performance/sessions_sync_perf_test.cc',
        'browser/sync/test/integration/performance/typed_urls_sync_perf_test.cc',
        'browser/sync/test/integration/sessions_helper.cc',
        'browser/sync/test/integration/sessions_helper.h',
        'browser/sync/test/integration/sync_datatype_helper.cc',
        'browser/sync/test/integration/sync_datatype_helper.h',
        'browser/sync/test/integration/sync_extension_helper.cc',
        'browser/sync/test/integration/sync_extension_helper.h',
        'browser/sync/test/integration/sync_test.cc',
        'browser/sync/test/integration/sync_test.h',
        'browser/sync/test/integration/typed_urls_helper.cc',
        'browser/sync/test/integration/typed_urls_helper.h',
        'test/base/chrome_test_launcher.cc',
        'test/data/resource.rc',
        '../content/app/startup_helper_win.cc',
        '../content/test/test_launcher.cc',
        '../content/test/test_launcher.h',
      ],
      'conditions': [
        ['toolkit_uses_gtk == 1', {
           'dependencies': [
             '../build/linux/system.gyp:gtk',
           ],
        }],
        ['toolkit_uses_gtk == 1 or chromeos==1 or (OS=="linux" and use_aura==1)', {
          'dependencies': [
            '../build/linux/system.gyp:ssl',
          ],
        }],
        ['OS=="mac"', {
          # The sync_integration_tests do not run on mac without this flag.
          # Search for comments about "xcode_settings" elsewhere in this file.
          'xcode_settings': {'OTHER_LDFLAGS': ['-Wl,-ObjC']},
        }],
        ['OS=="win"', {
          'sources': [
            '<(SHARED_INTERMEDIATE_DIR)/chrome/browser_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/common_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/extensions_api_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/theme_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/theme_resources_standard.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome_version/other_version.rc',
            '<(SHARED_INTERMEDIATE_DIR)/ui/ui_resources/ui_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/ui/ui_resources_standard/ui_resources_standard.rc',
          ],
          'include_dirs': [
            '<(DEPTH)/third_party/wtl/include',
          ],
          'dependencies': [
            'chrome_version_resources',
            'installer_util_strings',
            '../sandbox/sandbox.gyp:sandbox',
          ],
          'conditions': [
            ['win_use_allocator_shim==1', {
              'dependencies': [
                '<(allocator_target)',
              ],
            }],
          ],
          'configurations': {
            'Debug': {
              'msvs_settings': {
                'VCLinkerTool': {
                  'LinkIncremental': '<(msvs_debug_link_nonincremental)',
                },
              },
            },
          },
        }, { # else: OS != "win"
          'sources!': [
            'app/chrome_dll.rc',
            'app/chrome_version.rc.version',
            'test/data/resource.rc',
          ],
        }],
        ['toolkit_views==1', {
          'dependencies': [
            '../ui/views/views.gyp:views',
          ],
        }],
      ],
    },
    {
      # Executable that contains all the tests to be run on the GPU bots.
      'target_name': 'gpu_tests',
      'type': 'executable',
      'dependencies': [
        # Runtime dependencies
        '../third_party/mesa/mesa.gyp:osmesa',
      ],
      'includes': [
        'test/gpu/test_support_gpu.gypi'
      ],
      'sources': [
        'test/gpu/gpu_crash_browsertest.cc',
        'test/gpu/gpu_feature_browsertest.cc',
        'test/gpu/gpu_pixel_browsertest.cc',
        'test/gpu/webgl_conformance_tests.cc',
        'test/gpu/webgl_conformance_test_list_autogen.h',
      ],
    },
    {
      # Executable that contains a subset of the gpu tests which are run with a
      # software rasterizer.
      'target_name': 'soft_gpu_tests',
      'type': 'executable',
      'includes': [
        'test/gpu/test_support_gpu.gypi'
      ],
      'sources': [
        'test/gpu/gpu_pixel_browsertest.cc',
      ],
    },
  ],
  'conditions': [
    ['OS=="mac"', {
      'targets': [
        {
          # This is the mac equivalent of the security_tests target below. It
          # generates a framework bundle which bundles tests to be run in a
          # renderer process. The test code is built as a framework so it can be
          # run in the context of a renderer without shipping the code to end
          # users.
          'target_name': 'renderer_sandbox_tests',
          'type': 'shared_library',
          'product_name': 'Renderer Sandbox Tests',
          'mac_bundle': 1,
          'xcode_settings': {
            'INFOPLIST_FILE': 'test/security_tests/sandbox_tests_mac-Info.plist',
          },
          'sources': [
            'test/security_tests/renderer_sandbox_tests_mac.h',
            'test/security_tests/renderer_sandbox_tests_mac.mm',
          ],
          'include_dirs': [
            '..',
          ],
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/Cocoa.framework',
            ],
          },
        },  # target renderer_sandbox_tests
        {
          # Tests for Mac app launcher.
          'target_name': 'app_mode_app_tests',
          'type': 'executable',
          'product_name': 'app_mode_app_tests',
          'dependencies': [
            '../base/base.gyp:test_support_base',
            '../testing/gtest.gyp:gtest',
            'chrome.gyp:chrome',  # run time dependency
            'common_constants',
            'app_mode_app_support',
          ],
          'sources': [
            'common/mac/app_mode_chrome_locator_unittest.mm',
            'test/base/app_mode_app_tests.cc',
          ],
          'include_dirs': [
            '..',
          ],
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/CoreFoundation.framework',
              '$(SDKROOT)/System/Library/Frameworks/Foundation.framework',
            ],
          },
        },  # target app_mode_app_tests
      ],
    }],
    ['OS!="mac"', {
      'targets': [
        {
          'target_name': 'perf_tests',
          'type': 'executable',
          'dependencies': [
            'browser',
            'chrome_resources.gyp:chrome_resources',
            'chrome_resources.gyp:chrome_strings',
            'common',
            'renderer',
            '../content/content.gyp:content_gpu',
            '../content/content.gyp:test_support_content',
            '../base/base.gyp:base',
            '../base/base.gyp:test_support_base',
            '../base/base.gyp:test_support_perf',
            '../skia/skia.gyp:skia',
            '../testing/gtest.gyp:gtest',
            '../webkit/support/webkit_support.gyp:glue',
          ],
          'sources': [
            'browser/visitedlink/visitedlink_perftest.cc',
            'common/json_value_serializer_perftest.cc',
            'test/perf/perftests.cc',
            'test/perf/url_parse_perftest.cc',
          ],
          'conditions': [
            ['toolkit_uses_gtk == 1', {
              'dependencies': [
                '../build/linux/system.gyp:gtk',
                '../tools/xdisplaycheck/xdisplaycheck.gyp:xdisplaycheck',
              ],
            }],
            ['OS=="win"', {
              'configurations': {
                'Debug_Base': {
                  'msvs_settings': {
                    'VCLinkerTool': {
                      'LinkIncremental': '<(msvs_large_module_debug_link_mode)',
                    },
                  },
                },
              },
              'conditions': [
                ['win_use_allocator_shim==1', {
                  'dependencies': [
                    '<(allocator_target)',
                  ],
                }],
              ],
            }],
            ['toolkit_views==1', {
              'dependencies': [
                '../ui/views/views.gyp:views',
              ],
            }],
            ['os_posix == 1 and OS != "mac"', {
              'conditions': [
                ['linux_use_tcmalloc==1', {
                  'dependencies': [
                    '../base/allocator/allocator.gyp:allocator',
                  ],
                }],
              ],
            }],
          ],
        },
      ],
    },],  # OS!="mac"
    ['OS=="win"', {
      'targets': [
        {
          'target_name': 'generate_profile',
          'type': 'executable',
          'dependencies': [
            'test_support_common',
            'browser',
            'renderer',
            '../base/base.gyp:base',
            '../net/net.gyp:net_test_support',
            '../skia/skia.gyp:skia',
            '../sync/sync.gyp:syncapi_core',
          ],
          'include_dirs': [
            '..',
          ],
          'sources': [
            'tools/profiles/generate_profile.cc',
            'tools/profiles/thumbnail-inl.h',
          ],
          'conditions': [
            ['OS=="win"', {
              'conditions': [
                ['win_use_allocator_shim==1', {
                  'dependencies': [
                    '<(allocator_target)',
                  ],
                }],
              ],
              'configurations': {
                'Debug_Base': {
                  'msvs_settings': {
                    'VCLinkerTool': {
                      'LinkIncremental': '<(msvs_large_module_debug_link_mode)',
                    },
                  },
                },
              },
            }],
          ],
        },
        {
          'target_name': 'security_tests',
          'type': 'shared_library',
          'include_dirs': [
            '..',
          ],
          'sources': [
            'test/security_tests/ipc_security_tests.cc',
            'test/security_tests/ipc_security_tests.h',
            'test/security_tests/security_tests.cc',
            '../content/common/injection_test_dll.h',
            '../sandbox/tests/validation_tests/commands.cc',
            '../sandbox/tests/validation_tests/commands.h',
          ],
        },
      ]},  # 'targets'
    ],  # OS=="win"
    # If you change this condition, make sure you also change it in all.gyp
    # for the chromium_builder_qa target.
    ['enable_automation==1 and (OS=="mac" or OS=="win" or (os_posix==1 and target_arch==python_arch))', {
      'targets': [
        {
          # Documentation: http://dev.chromium.org/developers/testing/pyauto
          'target_name': 'pyautolib',
          'type': 'loadable_module',
          'product_prefix': '_',
          'dependencies': [
            'chrome',
            'chrome_resources.gyp:chrome_resources',
            'chrome_resources.gyp:chrome_strings',
            'chrome_resources.gyp:theme_resources',
            'debugger',
            'test_support_common',
            '../skia/skia.gyp:skia',
            '../sync/sync.gyp:syncapi_core',
            '../testing/gtest.gyp:gtest',
          ],
          'export_dependent_settings': [
            'test_support_common',
          ],
          'include_dirs': [
            '..',
          ],
          'cflags': [
             '-Wno-uninitialized',
             '-Wno-self-assign',  # to keep clang happy for generated code.
          ],
          'sources': [
            'test/automation/proxy_launcher.cc',
            'test/automation/proxy_launcher.h',
            'test/pyautolib/pyautolib.cc',
            'test/pyautolib/pyautolib.h',
            'test/ui/ui_test.cc',
            'test/ui/ui_test.h',
            'test/ui/ui_test_suite.cc',
            'test/ui/ui_test_suite.h',
            '<(INTERMEDIATE_DIR)/pyautolib_wrap.cc',
            '<@(pyautolib_sources)',
          ],
          'xcode_settings': {
            # Link with python2.6. Using -L/usr/lib and -lpython2.6 does not
            # work with the -isysroot argument passed in. Even if it did,
            # the linker shouldn't use any other lib not in the 10.5 sdk.
            'OTHER_LDFLAGS': [
              '/usr/lib/libpython2.6.dylib'
            ],
          },
          'msvs_disabled_warnings': [4211],
          'conditions': [
            ['os_posix == 1 and OS!="mac"', {
              'include_dirs': [
                '..',
                '<(sysroot)/usr/include/python<(python_ver)',
              ],
              'link_settings': {
                'libraries': [
                  '-lpython<(python_ver)',
                ],
              },
            }],
            ['toolkit_uses_gtk == 1', {
              'dependencies': [
                '../build/linux/system.gyp:gtk',
              ],
            }],
            ['OS=="mac"', {
              'include_dirs': [
                '..',
                '/usr/include/python2.6',
              ],
            }],
            ['OS=="win"', {
              'product_extension': 'pyd',
              'include_dirs': [
                '..',
                '../third_party/python_26/include',
              ],
              'link_settings': {
                'libraries': [
                  '../third_party/python_26/libs/python26.lib',
                ],
              }
            }],
            ['clang == 1', {
              'xcode_settings': {
                'WARNING_CFLAGS': [
                  # swig creates code with self assignments.
                  '-Wno-self-assign',
                ],
              },
              'cflags': [
                '-Wno-self-assign',
              ],
            }],
            ['asan==1', {
              'cflags!': [ '-faddress-sanitizer' ],
              'xcode_settings': {
                'OTHER_CFLAGS!': [
                  '-faddress-sanitizer',
                ],
              },
            }],
          ],
          'actions': [
            {
              'action_name': 'pyautolib_swig',
              'inputs': [
                'test/pyautolib/argc_argv.i',
                'test/pyautolib/pyautolib.i',
                '<@(pyautolib_sources)',
              ],
              'outputs': [
                '<(INTERMEDIATE_DIR)/pyautolib_wrap.cc',
                '<(PRODUCT_DIR)/pyautolib.py',
              ],
              'action': [ 'python',
                          '../tools/swig/swig.py',
                          '-I..',
                          '-python',
                          '-c++',
                          '-threads',
                          '-outdir',
                          '<(PRODUCT_DIR)',
                          '-o',
                          '<(INTERMEDIATE_DIR)/pyautolib_wrap.cc',
                          'test/pyautolib/pyautolib.i',
              ],
              'message': 'Generating swig wrappers for pyautolib.',
            },
          ],  # actions
        },  # target 'pyautolib'
      ]  # targets
    }],
    # To enable the coverage targets, do
    #    GYP_DEFINES='coverage=1' gclient sync
    # To match the coverage buildbot more closely, do this:
    #    GYP_DEFINES='coverage=1 enable_svg=0 fastbuild=1' gclient sync
    # (and, on MacOS, be sure to switch your SDK from "Base SDK" to "Mac OS X
    # 10.6")
    # (but on Windows, don't set the fastbuild=1 because it removes the PDB
    # generation which is necessary for code coverage.)
    ['coverage!=0',
      { 'targets': [
        {
          ### Coverage BUILD AND RUN.
          ### Not named coverage_build_and_run for historical reasons.
          'target_name': 'coverage',
          'dependencies': [ 'coverage_build', 'coverage_run' ],
          # do NOT place this in the 'all' list; most won't want it.
          # In gyp, booleans are 0/1 not True/False.
          'suppress_wildcard': 1,
          'type': 'none',
          'actions': [
            {
              'message': 'Coverage is now complete.',
              # MSVS must have an input file and an output file.
              'inputs': [ '<(PRODUCT_DIR)/coverage.info' ],
              'outputs': [ '<(PRODUCT_DIR)/coverage-build-and-run.stamp' ],
              'action_name': 'coverage',
              # Wish gyp had some basic builtin commands (e.g. 'touch').
              'action': [ 'python', '-c',
                          'import os; ' \
                          'open(' \
                          '\'<(PRODUCT_DIR)\' + os.path.sep + ' \
                          '\'coverage-build-and-run.stamp\'' \
                          ', \'w\').close()' ],
              # Use outputs of this action as inputs for the main target build.
              # Seems as a misnomer but makes this happy on Linux (scons).
              'process_outputs_as_sources': 1,
            },
          ],  # 'actions'
        },
        ### Coverage BUILD.  Compile only; does not run the bundles.
        ### Intended as the build phase for our coverage bots.
        ###
        ### Builds unit test bundles needed for coverage.
        ### Outputs this list of bundles into coverage_bundles.py.
        ###
        ### If you want to both build and run coverage from your IDE,
        ### use the 'coverage' target.
        {
          'target_name': 'coverage_build',
          # do NOT place this in the 'all' list; most won't want it.
          # In gyp, booleans are 0/1 not True/False.
          'suppress_wildcard': 1,
          'type': 'none',
          'dependencies': [
            'automated_ui_tests',
            '../base/base.gyp:base_unittests',
            # browser_tests's use of subprocesses chokes gcov on 10.6?
            # Disabling for now (enabled on linux/windows below).
            # 'browser_tests',
            '../ipc/ipc.gyp:ipc_tests',
            '../media/media.gyp:media_unittests',
            '../net/net.gyp:net_unittests',
            '../printing/printing.gyp:printing_unittests',
            '../remoting/remoting.gyp:remoting_unittests',
            '../sql/sql.gyp:sql_unittests',
            '../content/content.gyp:content_unittests',
            'unit_tests',
            'sync_unit_tests',
          ],  # 'dependencies'
          'conditions': [
            ['OS=="win"', {
              'dependencies': [
                # Courgette has not been ported from Windows.
                # Note build/win/chrome_win.croc uniquely has the
                # courgette source directory in an include path.
                '../courgette/courgette.gyp:courgette_unittests',
                'browser_tests',
                ]}],
            ['OS=="linux"', {
              'dependencies': [
                # Win bot needs to be turned into an interactive bot.
                'interactive_ui_tests',
                'browser_tests',
              ]}],
            ['OS=="mac"', {
              'dependencies': [
              # Placeholder; empty for now.
              ]}],
          ],  # 'conditions'
          'actions': [
            {
              # 'message' for Linux/scons in particular.  Scons
              # requires the 'coverage' target be run from within
              # src/chrome.
              'message': 'Compiling coverage bundles.',
              # MSVS must have an input file and an output file.
              #
              # TODO(jrg):
              # Technically I want inputs to be the list of
              # executables created in <@(_dependencies) but use of
              # that variable lists the dep by dep name, not their
              # output executable name.
              # Is there a better way to force this action to run, always?
              #
              # If a test bundle is added to this coverage_build target it
              # necessarily means this file (chrome_tests.gypi) is changed,
              # so the action is run (coverage_bundles.py is generated).
              # Exceptions to that rule are theoretically possible
              # (e.g. re-gyp with a GYP_DEFINES set).
              # Else it's the same list of bundles as last time.  They are
              # built (since on the deps list) but the action may not run.
              # For now, things work, but it's less than ideal.
              'inputs': [ 'chrome_tests.gypi' ],
              'outputs': [ '<(PRODUCT_DIR)/coverage_bundles.py' ],
              'action_name': 'coverage_build',
              'action': [ 'python', '-c',
                          'import os; '
                          'f = open(' \
                          '\'<(PRODUCT_DIR)\' + os.path.sep + ' \
                          '\'coverage_bundles.py\'' \
                          ', \'w\'); ' \
                          'deplist = \'' \
                          '<@(_dependencies)' \
                          '\'.split(\' \'); ' \
                          'f.write(str(deplist)); ' \
                          'f.close()'],
              # Use outputs of this action as inputs for the main target build.
              # Seems as a misnomer but makes this happy on Linux (scons).
              'process_outputs_as_sources': 1,
            },
          ],  # 'actions'
        },
        ### Coverage RUN.  Does not compile the bundles.  Mirrors the
        ### run_coverage_bundles buildbot phase.  If you update this
        ### command update the mirror in
        ### $BUILDBOT/scripts/master/factory/chromium_commands.py.
        ### If you want both build and run, use the 'coverage' target.
        {
          'target_name': 'coverage_run',
          # do NOT place this in the 'all' list; most won't want it.
          # In gyp, booleans are 0/1 not True/False.
          'suppress_wildcard': 1,
          'type': 'none',
          'actions': [
            {
              # 'message' for Linux/scons in particular.  Scons
              # requires the 'coverage' target be run from within
              # src/chrome.
              'message': 'Running the coverage script.  NOT building anything.',
              # MSVS must have an input file and an output file.
              'inputs': [ '<(PRODUCT_DIR)/coverage_bundles.py' ],
              'outputs': [ '<(PRODUCT_DIR)/coverage.info' ],
              'action_name': 'coverage_run',
              'action': [ 'python',
                          '../tools/code_coverage/coverage_posix.py',
                          '--directory',
                          '<(PRODUCT_DIR)',
                          '--src_root',
                          '..',
                          '--bundles',
                          '<(PRODUCT_DIR)/coverage_bundles.py'],
              # Use outputs of this action as inputs for the main target build.
              # Seems as a misnomer but makes this happy on Linux (scons).
              'process_outputs_as_sources': 1,
            },
          ],  # 'actions'
        },
      ]
    }],  # 'coverage!=0'
  ],  # 'conditions'
}
