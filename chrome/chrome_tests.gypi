# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'target_defaults': {
    'conditions': [
      ['strip_tests==1', {
        'cflags!': [ '-g' ],
        'ldflags': [ '-Wl,--strip-debug' ],
      }],
    ],
  },
  'variables' : {
    # Variables for js2gtest rules
    'gypv8sh': '../tools/gypv8sh.py',
    'js2gtest': 'test/base/js2gtest.js',
    'mock_js': 'third_party/mock4js/mock4js.js',
    'test_api_js': 'test/data/webui/test_api.js',

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
    ],
    'pyautolib_libraries': [
    ],
    'conditions': [
      ['asan==1', {
        'pyautolib_libraries': [
          # Link in the libasan32.a because this binary will be loaded by
          # Python that does not have libasan in.
          '-lasan32',
        ]
      }],
    ],
  },
  'targets': [
    {
      # This target contains mocks and test utilities that don't belong in
      # production libraries but are used by more than one test executable.
      'target_name': 'test_support_common',
      'type': 'static_library',
      'dependencies': [
        'app/policy/cloud_policy_codegen.gyp:policy',
        'browser',
        'browser/sync/protocol/sync_proto.gyp:sync_proto',
        'chrome_resources.gyp:chrome_resources',
        'chrome_resources.gyp:chrome_strings',
        'chrome_resources.gyp:theme_resources',
        'common',
        'plugin',
        'renderer',
        'service',
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
        # 'test/base/test_url_request_context_getter.h' brings in this requirement.
        '../net/net.gyp:net_test_support',
        '../skia/skia.gyp:skia',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        '../third_party/leveldatabase/leveldatabase.gyp:leveldatabase',
        '../ui/gfx/compositor/compositor.gyp:test_compositor',
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
        'browser/automation/mock_tab_event_observer.cc',
        'browser/automation/mock_tab_event_observer.h',
        'browser/chromeos/cros/mock_cryptohome_library.cc',
        'browser/chromeos/cros/mock_cryptohome_library.h',
        'browser/chromeos/cros/mock_library_loader.cc',
        'browser/chromeos/cros/mock_library_loader.h',
        'browser/chromeos/cros/mock_network_library.cc',
        'browser/chromeos/cros/mock_network_library.h',
        'browser/chromeos/dbus/mock_bluetooth_adapter_client.cc',
        'browser/chromeos/dbus/mock_bluetooth_adapter_client.h',
        'browser/chromeos/dbus/mock_bluetooth_manager_client.cc',
        'browser/chromeos/dbus/mock_bluetooth_manager_client.h',
        'browser/chromeos/dbus/mock_cros_disks_client.cc',
        'browser/chromeos/dbus/mock_cros_disks_client.h',
        'browser/chromeos/dbus/mock_dbus_thread_manager.cc',
        'browser/chromeos/dbus/mock_dbus_thread_manager.h',
        'browser/chromeos/dbus/mock_power_manager_client.cc',
        'browser/chromeos/dbus/mock_power_manager_client.h',
        'browser/chromeos/dbus/mock_sensors_client.cc',
        'browser/chromeos/dbus/mock_sensors_client.h',
        'browser/chromeos/dbus/mock_session_manager_client.cc',
        'browser/chromeos/dbus/mock_session_manager_client.h',
        'browser/chromeos/dbus/mock_speech_synthesizer_client.cc',
        'browser/chromeos/dbus/mock_speech_synthesizer_client.h',
        'browser/chromeos/dbus/mock_update_engine_client.cc',
        'browser/chromeos/dbus/mock_update_engine_client.h',
        'browser/chromeos/login/mock_signed_settings_helper.cc',
        'browser/chromeos/login/mock_signed_settings_helper.h',
        # The only thing used from browser is Browser::Type.
        'browser/extensions/mock_extension_special_storage_policy.cc',
        'browser/extensions/mock_extension_special_storage_policy.h',
        'browser/extensions/test_extension_prefs.cc',
        'browser/extensions/test_extension_prefs.h',
        'browser/extensions/test_extension_service.cc',
        'browser/extensions/test_extension_service.h',
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
        'browser/notifications/notification_test_util.cc',
        'browser/notifications/notification_test_util.h',
        'browser/policy/mock_cloud_policy_data_store.cc',
        'browser/policy/mock_cloud_policy_data_store.h',
        'browser/policy/mock_device_management_service.cc',
        'browser/policy/mock_device_management_service.h',
        'browser/prefs/pref_observer_mock.cc',
        'browser/prefs/pref_observer_mock.h',
        'browser/prefs/pref_service_mock_builder.cc',
        'browser/prefs/pref_service_mock_builder.h',
        'browser/prefs/testing_pref_store.cc',
        'browser/prefs/testing_pref_store.h',
        'browser/search_engines/template_url_service_test_util.cc',
        'browser/search_engines/template_url_service_test_util.h',
        'browser/sessions/session_service_test_helper.cc',
        'browser/sessions/session_service_test_helper.h',
        'browser/sync/profile_sync_service_mock.cc',
        'browser/sync/profile_sync_service_mock.h',
        'browser/sync/syncable/syncable_mock.cc',
        'browser/sync/syncable/syncable_mock.h',
        'browser/ui/browser.h',
        'browser/ui/panels/base_panel_browser_test.cc',
        'browser/ui/panels/base_panel_browser_test.h',
        'browser/ui/tab_contents/test_tab_contents_wrapper.cc',
        'browser/ui/tab_contents/test_tab_contents_wrapper.h',
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
        'test/automation/extension_proxy.cc',
        'test/automation/extension_proxy.h',
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
        'test/base/profile_mock.cc',
        'test/base/profile_mock.h',
        'test/base/test_browser_window.cc',
        'test/base/test_browser_window.h',
        'test/base/test_html_dialog_observer.cc',
        'test/base/test_html_dialog_observer.h',
        'test/base/test_launcher_utils.cc',
        'test/base/test_launcher_utils.h',
        'test/base/test_location_bar.cc',
        'test/base/test_location_bar.h',
        'test/base/test_switches.cc',
        'test/base/test_switches.h',
        'test/base/test_tab_strip_model_observer.cc',
        'test/base/test_tab_strip_model_observer.h',
        'test/base/test_url_request_context_getter.cc',
        'test/base/test_url_request_context_getter.h',
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
        'test/base/values_test_util.cc',
        'test/base/values_test_util.h',
        'test/test_navigation_observer.cc',
        'test/test_navigation_observer.h',
        # TODO:  these should live here but are currently used by
        # production code code in libbrowser (in chrome.gyp).
        #'../content/browser/net/url_request_mock_http_job.cc',
        #'../content/browser/net/url_request_mock_http_job.h',
        '../content/browser/ssl/ssl_client_auth_handler_mock.h',
        '../content/test/notification_observer_mock.cc',
        '../content/test/notification_observer_mock.h',
        '../ui/gfx/image/image_unittest_util.h',
        '../ui/gfx/image/image_unittest_util.cc',
        '../webkit/quota/mock_quota_manager.cc',
        '../webkit/quota/mock_quota_manager.h',
      ],
      'conditions': [
        ['chromeos==0', {
          'sources/': [
            ['exclude', '^browser/chromeos'],
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
        'test/ui/npapi_test_helper.cc',
        'test/ui/npapi_test_helper.h',
        'test/ui/run_all_unittests.cc',
        'test/ui/ui_layout_test.cc',
        'test/ui/ui_layout_test.h',
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
      'target_name': 'test_support_sync',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        'sync',
      ],
      'export_dependent_settings': [
        '../base/base.gyp:base',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        'sync',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'browser/sync/js/js_test_util.cc',
        'browser/sync/js/js_test_util.h',
        'browser/sync/test/null_directory_change_delegate.cc',
        'browser/sync/test/null_directory_change_delegate.h',
        'browser/sync/test/null_transaction_observer.cc',
        'browser/sync/test/null_transaction_observer.h',
        'browser/sync/test/engine/test_directory_setter_upper.cc',
        'browser/sync/test/engine/test_directory_setter_upper.h',
      ],
    },
    {
      'target_name': 'test_support_syncapi',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        'syncapi_core',
        'test_support_sync',
      ],
      'export_dependent_settings': [
        '../base/base.gyp:base',
        'syncapi_core',
        'test_support_sync',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'browser/sync/test/engine/test_user_share.cc',
        'browser/sync/test/engine/test_user_share.h',
      ],
    },
    {
      'target_name': 'test_support_syncapi_service',
      'type': 'static_library',
      'dependencies': [
        '../testing/gmock.gyp:gmock',
        'syncapi_service',
      ],
      'export_dependent_settings': [
        '../testing/gmock.gyp:gmock',
        'syncapi_service',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'browser/sync/api/syncable_service_mock.cc',
        'browser/sync/api/syncable_service_mock.h',
      ],
    },
    {
      'target_name': 'test_support_sync_notifier',
      'type': 'static_library',
      'dependencies': [
        '../testing/gmock.gyp:gmock',
        'sync_notifier',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'browser/sync/notifier/mock_sync_notifier_observer.cc',
        'browser/sync/notifier/mock_sync_notifier_observer.h',
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
        'browser/sync/protocol/sync_proto.gyp:sync_proto',
        'chrome',
        'chrome_resources.gyp:chrome_resources',
        'chrome_resources.gyp:chrome_strings',
        'debugger',
        'syncapi_core',
        'test_support_common',
        'test_support_ui',
        '../third_party/hunspell/hunspell.gyp:hunspell',
        '../net/net.gyp:net',
        '../net/net.gyp:net_resources',
        '../net/net.gyp:net_test_support',
        '../skia/skia.gyp:skia',
        '../third_party/icu/icu.gyp:icui18n',
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
        'browser/accessibility/accessibility_mac_uitest.mm',
        'browser/autofill/autofill_browsertest.cc',
        'browser/browser_focus_uitest.cc',
        'browser/browser_keyevents_browsertest.cc',
        'browser/collected_cookies_uitest.cc',
        'browser/debugger/devtools_sanity_unittest.cc',
        'browser/infobars/infobars_uitest.cc',
        'browser/instant/instant_browsertest.cc',
        'browser/keyboard_access_uitest.cc',
        'browser/mouseleave_interactive_uitest.cc',
        'browser/notifications/notifications_interactive_uitest.cc',
        'browser/npapi_interactive_test.cc',
        'browser/ui/gtk/bookmarks/bookmark_bar_gtk_interactive_uitest.cc',
        'browser/ui/omnibox/omnibox_view_browsertest.cc',
        'browser/ui/panels/panel_browsertest.cc',
        'browser/ui/views/bookmarks/bookmark_bar_view_test.cc',
        'browser/ui/views/button_dropdown_test.cc',
        'browser/ui/views/constrained_window_views_browsertest.cc',
        'browser/ui/views/find_bar_host_interactive_uitest.cc',
        'browser/ui/views/menu_item_view_test.cc',
        'browser/ui/views/menu_model_adapter_test.cc',
        'browser/ui/views/ssl_client_certificate_selector_browsertest.cc',
        'browser/ui/views/tabs/tab_dragging_test.cc',
        'browser/ui/webui/workers_ui_browsertest.cc',
        'test/base/chrome_test_launcher.cc',
        'test/base/view_event_test_base.cc',
        'test/base/view_event_test_base.h',
        '../content/app/startup_helper_win.cc',
        '../content/test/test_launcher.cc',
        '../content/test/test_launcher.h',
      ],
      'conditions': [
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
        ['toolkit_uses_gtk == 1 and toolkit_views == 0', {
          'sources!': [
            # TODO(port)
            'browser/npapi_interactive_test.cc',
            'browser/ui/views/bookmarks/bookmark_bar_view_test.cc',
            'browser/ui/views/button_dropdown_test.cc',
            'browser/ui/views/find_bar_host_interactive_uitest.cc',
            'browser/ui/views/menu_item_view_test.cc',
            'browser/ui/views/menu_model_adapter_test.cc',
            'browser/ui/views/tabs/tab_dragging_test.cc',
            'browser/ui/views/tabs/tab_strip_interactive_uitest.cc',
            'test/base/view_event_test_base.cc',
            'test/base/view_event_test_base.h',
          ],
        }],
        ['OS=="linux" and toolkit_views==1', {
          'sources!': [
            # TODO(port)
            'browser/npapi_interactive_test.cc',

            'browser/ui/gtk/bookmarks/bookmark_bar_gtk_interactive_uitest.cc',
          ],
        }],
        ['target_arch!="arm"', {
          'dependencies': [
            # run time dependency
            '../webkit/webkit.gyp:npapi_test_plugin',
          ],
        }],  # target_arch
        ['OS=="mac"', {
          'sources!': [
            # TODO(port)
            'browser/npapi_interactive_test.cc',
            'browser/ui/views/bookmarks/bookmark_bar_view_test.cc',
            'browser/ui/views/button_dropdown_test.cc',
            'browser/ui/views/find_bar_host_interactive_uitest.cc',
            'browser/ui/views/menu_item_view_test.cc',
            'browser/ui/views/menu_model_adapter_test.cc',
            'browser/ui/views/tabs/tab_dragging_test.cc',
            'browser/ui/views/tabs/tab_strip_interactive_uitest.cc',
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
            '../views/views.gyp:views',
          ],
        }],
        ['chromeos==1', {
          'sources!': [
            'browser/ui/panels/panel_browsertest.cc',
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
            '<(SHARED_INTERMEDIATE_DIR)/chrome/renderer_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/theme_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/theme_resources_standard.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome_version/other_version.rc',
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
            'browser/ui/views/constrained_window_views_browsertest.cc',
            'browser/ui/views/ssl_client_certificate_selector_browsertest.cc',
          ],
        }],  # OS != "win"
      ],  # conditions
    },
    {
      'target_name': 'ui_tests',
      'type': 'executable',
      'dependencies': [
        'browser',
        'chrome',
        'chrome_resources.gyp:chrome_resources',
        'chrome_resources.gyp:chrome_strings',
        'common',
        'test_support_ui',
        '../base/base.gyp:base',
        '../net/net.gyp:net',
        '../net/net.gyp:net_test_support',
        '../build/temp_gyp/googleurl.gyp:googleurl',
        '../skia/skia.gyp:skia',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        '../third_party/icu/icu.gyp:icui18n',
        '../third_party/icu/icu.gyp:icuuc',
        '../third_party/libxml/libxml.gyp:libxml',
        # run time dependencies
        'default_plugin/default_plugin.gyp:default_plugin',
        '../ppapi/ppapi_internal.gyp:ppapi_tests',
        '../third_party/mesa/mesa.gyp:osmesa',
        '../third_party/WebKit/Source/WebKit/chromium/WebKit.gyp:copy_TestNetscapePlugIn',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'app/chrome_main_uitest.cc',
        'browser/browser_encoding_uitest.cc',
        'browser/custom_handlers/custom_handlers_uitest.cc',
        'browser/download/save_page_uitest.cc',
        'browser/default_plugin_uitest.cc',
        'browser/fast_shutdown_uitest.cc',
        'browser/history/multipart_uitest.cc',
        'browser/history/redirect_uitest.cc',
        'browser/iframe_uitest.cc',
        'browser/images_uitest.cc',
        'browser/locale_tests_uitest.cc',
        'browser/media_uitest.cc',
        'browser/metrics/metrics_service_uitest.cc',
        'browser/net/load_timing_observer_uitest.cc',
        'browser/prefs/pref_service_uitest.cc',
        'browser/printing/printing_layout_uitest.cc',
        'browser/process_singleton_linux_uitest.cc',
        'browser/process_singleton_uitest.cc',
        'browser/repost_form_warning_uitest.cc',
        'browser/sanity_uitest.cc',
        'browser/session_history_uitest.cc',
        'browser/sessions/session_restore_uitest.cc',
        'browser/tab_contents/view_source_uitest.cc',
        'browser/tab_restore_uitest.cc',
        'browser/unload_uitest.cc',
        'browser/ui/login/login_prompt_uitest.cc',
        'browser/ui/tests/browser_uitest.cc',
        'browser/ui/views/find_bar_host_uitest.cc',
        'browser/ui/webui/bookmarks_ui_uitest.cc',
        'browser/ui/webui/ntp/new_tab_ui_uitest.cc',
        'browser/ui/webui/options/chromeos/guest_mode_options_ui_uitest.cc',
        'browser/ui/webui/options/options_ui_uitest.cc',
        'browser/ui/webui/options/options_ui_uitest.h',
        'browser/ui/webui/print_preview_ui_uitest.cc',
        'common/chrome_switches_uitest.cc',
        'common/logging_chrome_uitest.cc',
        'renderer/external_extension_uitest.cc',
        'renderer/loadtimes_extension_bindings_uitest.cc',
        'test/automation/automation_proxy_uitest.cc',
        'test/automation/automation_proxy_uitest.h',
        'test/automation/extension_proxy_uitest.cc',
        'test/base/chrome_process_util_uitest.cc',
        'test/reliability/automated_ui_test_test.cc',
        'test/ui/history_uitest.cc',
        'test/ui/layout_plugin_uitest.cc',
        'test/ui/named_interface_uitest.cc',
        'test/ui/npapi_uitest.cc',
        'test/ui/ppapi_uitest.cc',
        'test/ui/sandbox_uitests.cc',
        '../content/browser/appcache/appcache_ui_test.cc',
        '../content/browser/in_process_webkit/dom_storage_uitest.cc',
        '../content/browser/renderer_host/resource_dispatcher_host_uitest.cc',
        '../content/worker/test/worker_uitest.cc',
      ],
      'conditions': [
        ['disable_nacl!=1', {
          'dependencies': [
            '../ppapi/ppapi_internal.gyp:ppapi_nacl_tests',
          ],
          'copies': [
            {
              'destination': '<(PRODUCT_DIR)',
              'files': [
                '../ppapi/tests/test_case.html',
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
        }],
        ['target_arch!="arm"', {
          'dependencies': [
            '../webkit/webkit.gyp:copy_npapi_test_plugin',
          ],
        }],
        ['toolkit_uses_gtk == 1', {
          'dependencies': [
            '../build/linux/system.gyp:gtk',
            '../tools/xdisplaycheck/xdisplaycheck.gyp:xdisplaycheck',
          ],
        }, { # else: toolkit_uses_gtk != 1
          'sources!': [
            'browser/process_singleton_linux_uitest.cc',
          ],
        }],
        ['toolkit_views==1', {
          'dependencies': [
            '../views/views.gyp:views',
          ],
        }],
        ['use_aura==1', {
          'sources!': [
            'test/automation/automation_proxy_uitest.cc',
            'test/ui/npapi_uitest.cc',
          ],
          'dependencies': [
            '../ui/aura/aura.gyp:aura',
          ],
        }],
        ['OS=="mac"', {
          # See the comment in this section of the unit_tests target for an
          # explanation (crbug.com/43791 - libwebcore.a is too large to mmap).
          'dependencies+++': [
            '../third_party/WebKit/Source/WebCore/WebCore.gyp/WebCore.gyp:webcore',
          ],
          'sources!': [
            # ProcessSingletonMac doesn't do anything.
            'browser/process_singleton_uitest.cc',
          ],
        }],
        ['OS=="win"', {
          'include_dirs': [
            '<(DEPTH)/third_party/wtl/include',
          ],
          'dependencies': [
            'security_tests',  # run time dependency
            'test_support_common',
            '../google_update/google_update.gyp:google_update',
          ],
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
            ],
          },
          'configurations': {
            'Debug_Base': {
              'msvs_settings': {
                'VCLinkerTool': {
                  'LinkIncremental': '<(msvs_large_module_debug_link_mode)',
                },
              },
            },
          },
        }, { # else: OS != "win"
          'sources!': [
            # TODO(port): http://crbug.com/45770
            'browser/printing/printing_layout_uitest.cc',
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
        ['chromeos==1', {
          'sources!': [
              # TODO(thestig): Enable when print preview is ready for CrOS.
             'browser/ui/webui/print_preview_ui_uitest.cc',
          ],
        }, { # else: chromeos == 0
          'sources/': [
            ['exclude', '^browser/ui/webui/options/chromeos/'],
          ],
        }],
      ],
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
        'syncapi_core',
        'test_support_ui',
        '../base/base.gyp:base',
        '../build/temp_gyp/googleurl.gyp:googleurl',
        '../net/net.gyp:net',
        '../skia/skia.gyp:skia',
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
        'test/webdriver/commands/find_element_commands.cc',
        'test/webdriver/commands/find_element_commands.h',
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
        'test/webdriver/frame_path.cc',
        'test/webdriver/frame_path.h',
        'test/webdriver/http_response.cc',
        'test/webdriver/http_response.h',
        'test/webdriver/keycode_text_conversion.h',
        'test/webdriver/keycode_text_conversion_linux.cc',
        'test/webdriver/keycode_text_conversion_mac.mm',
        'test/webdriver/keycode_text_conversion_win.cc',
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
        }],
        ['OS=="linux" and toolkit_views==1', {
          'dependencies': [
            '../views/views.gyp:views',
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
        '../base/base.gyp:test_support_base',
        '../testing/gtest.gyp:gtest',
        '../skia/skia.gyp:skia',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        '../base/test/run_all_unittests.cc',
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
        '../gpu/gpu.gyp:gpu_unittest_utils',
        '../jingle/jingle.gyp:jingle_glue_test_util',
        '../media/media.gyp:media_test_support',
        '../net/net.gyp:net',
        '../net/net.gyp:net_test_support',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        'test_support_common',
        'test_support_sync',
        'test_support_syncapi',
        'test_support_syncapi_service',
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
        '../ui/gfx/gl/gl.gyp:gl',
        '../ui/ui.gyp:ui_resources',
        '../ui/ui.gyp:ui_resources_standard',
        '../ui/ui.gyp:ui_test_support',
        '../v8/tools/gyp/v8.gyp:v8',
        'chrome_resources.gyp:chrome_resources',
        'chrome_resources.gyp:chrome_strings',
      ],
      'include_dirs': [
        '..',
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
        'browser/accessibility/browser_accessibility_mac_unittest.mm',
        'browser/accessibility/browser_accessibility_manager_unittest.cc',
        'browser/accessibility/browser_accessibility_win_unittest.cc',
        'browser/app_controller_mac_unittest.mm',
        'browser/autocomplete/autocomplete_edit_unittest.cc',
        'browser/autocomplete/autocomplete_popup_model_unittest.cc',
        'browser/autocomplete/autocomplete_result_unittest.cc',
        'browser/autocomplete/autocomplete_unittest.cc',
        'browser/autocomplete/builtin_provider_unittest.cc',
        'browser/autocomplete/extension_app_provider_unittest.cc',
        'browser/autocomplete/history_contents_provider_unittest.cc',
        'browser/autocomplete/history_quick_provider_unittest.cc',
        'browser/autocomplete/history_url_provider_unittest.cc',
        'browser/autocomplete/keyword_provider_unittest.cc',
        'browser/autocomplete/network_action_predictor_unittest.cc',
        'browser/autocomplete/network_action_predictor_database_unittest.cc',
        'browser/autocomplete/search_provider_unittest.cc',
        'browser/autocomplete/shortcuts_provider_unittest.cc',
        'browser/autocomplete_history_manager_unittest.cc',
        'browser/autofill/address_field_unittest.cc',
        'browser/autofill/address_unittest.cc',
        'browser/autofill/autofill_country_unittest.cc',
        'browser/autofill/autofill_download_unittest.cc',
        'browser/autofill/autofill_field_unittest.cc',
        'browser/autofill/autofill_ie_toolbar_import_win_unittest.cc',
        'browser/autofill/autofill_manager_unittest.cc',
        'browser/autofill/autofill_merge_unittest.cc',
        'browser/autofill/autofill_metrics_unittest.cc',
        'browser/autofill/autofill_profile_unittest.cc',
        'browser/autofill/autofill_type_unittest.cc',
        'browser/autofill/autofill_xml_parser_unittest.cc',
        'browser/autofill/contact_info_unittest.cc',
        'browser/autofill/credit_card_field_unittest.cc',
        'browser/autofill/credit_card_unittest.cc',
        'browser/autofill/form_field_unittest.cc',
        'browser/autofill/form_structure_unittest.cc',
        'browser/autofill/name_field_unittest.cc',
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
        'browser/browsing_data_indexed_db_helper_unittest.cc',
        'browser/browsing_data_local_storage_helper_unittest.cc',
        'browser/browsing_data_quota_helper_unittest.cc',
        'browser/browsing_data_remover_unittest.cc',
        'browser/chrome_browser_application_mac_unittest.mm',
        'browser/chrome_browser_main_unittest.cc',
        'browser/chrome_page_zoom_unittest.cc',
        'browser/chromeos/cros/network_library.cc',
        'browser/chromeos/cros/network_library.h',
        'browser/chromeos/cros/network_library_unittest.cc',
        'browser/chromeos/cros/onc_network_parser_unittest.cc',
        'browser/chromeos/customization_document_unittest.cc',
        'browser/chromeos/dbus/cros_dbus_service_unittest.cc',
        'browser/chromeos/dbus/proxy_resolution_service_provider_unittest.cc',
        'browser/chromeos/extensions/file_browser_notifications_unittest.cc',
        'browser/chromeos/external_metrics_unittest.cc',
        'browser/chromeos/gview_request_interceptor_unittest.cc',
        'browser/chromeos/input_method/candidate_window_view_unittest.cc',
        'browser/chromeos/input_method/hotkey_manager_unittest.cc',
        'browser/chromeos/input_method/ibus_controller_unittest.cc',
        'browser/chromeos/input_method/input_method_util_unittest.cc',
        'browser/chromeos/input_method/virtual_keyboard_selector_unittest.cc',
        'browser/chromeos/input_method/xkeyboard_unittest.cc',
        'browser/chromeos/language_preferences_unittest.cc',
        'browser/chromeos/login/authenticator_unittest.cc',
        'browser/chromeos/login/cookie_fetcher_unittest.cc',
        'browser/chromeos/login/cryptohome_op_unittest.cc',
        'browser/chromeos/login/mock_auth_attempt_state_resolver.cc',
        'browser/chromeos/login/mock_auth_attempt_state_resolver.h',
        'browser/chromeos/login/mock_auth_response_handler.cc',
        'browser/chromeos/login/mock_login_status_consumer.cc',
        'browser/chromeos/login/mock_login_status_consumer.h',
        'browser/chromeos/login/mock_owner_key_utils.cc',
        'browser/chromeos/login/mock_owner_key_utils.h',
        'browser/chromeos/login/mock_ownership_service.cc',
        'browser/chromeos/login/mock_ownership_service.h',
        'browser/chromeos/login/mock_url_fetchers.cc',
        'browser/chromeos/login/mock_url_fetchers.h',
        'browser/chromeos/login/mock_user_manager.cc',
        'browser/chromeos/login/mock_user_manager.h',
        'browser/chromeos/login/online_attempt_unittest.cc',
        'browser/chromeos/login/owner_key_utils_unittest.cc',
        'browser/chromeos/login/owner_manager_unittest.cc',
        'browser/chromeos/login/ownership_service_unittest.cc',
        'browser/chromeos/login/parallel_authenticator_unittest.cc',
        'browser/chromeos/login/signed_settings_helper_unittest.cc',
        'browser/chromeos/login/signed_settings_temp_storage_unittest.cc',
        'browser/chromeos/login/signed_settings_unittest.cc',
        'browser/chromeos/login/wizard_accessibility_handler_unittest.cc',
        'browser/chromeos/mobile_config_unittest.cc',
        'browser/chromeos/network_message_observer_unittest.cc',
        'browser/chromeos/notifications/desktop_notifications_unittest.cc',
        'browser/chromeos/offline/offline_load_page_unittest.cc',
        'browser/chromeos/proxy_config_service_impl_unittest.cc',
        'browser/chromeos/status/input_method_menu_unittest.cc',
        'browser/chromeos/status/network_menu_icon.cc',
        'browser/chromeos/status/network_menu_icon_unittest.cc',
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
        'browser/debugger/devtools_remote_listen_socket_unittest.cc',
        'browser/debugger/devtools_remote_listen_socket_unittest.h',
        'browser/debugger/devtools_remote_message_unittest.cc',
        'browser/diagnostics/diagnostics_model_unittest.cc',
        'browser/download/download_item_unittest.cc',
        'browser/download/download_manager_unittest.cc',
        'browser/download/download_request_infobar_delegate_unittest.cc',
        'browser/download/download_request_limiter_unittest.cc',
        'browser/enumerate_modules_model_unittest_win.cc',
        'browser/event_disposition.cc',
        'browser/event_disposition.h',
        'browser/extensions/app_notification_manager_sync_unittest.cc',
        'browser/extensions/app_notification_manager_unittest.cc',
        'browser/extensions/app_notification_storage_unittest.cc',
        'browser/extensions/app_notification_test_util.cc',
        'browser/extensions/app_notify_channel_setup_unittest.cc',
        'browser/extensions/apps_promo_unittest.cc',
        'browser/extensions/component_loader_unittest.cc',
        'browser/extensions/convert_user_script_unittest.cc',
        'browser/extensions/convert_web_app_unittest.cc',
        'browser/extensions/extension_content_settings_store_unittest.cc',
        'browser/extensions/extension_content_settings_unittest.cc',
        'browser/extensions/extension_cookies_unittest.cc',
        'browser/extensions/extension_creator_filter_unittest.cc',
        'browser/extensions/extension_event_router_forwarder_unittest.cc',
        'browser/extensions/extension_icon_manager_unittest.cc',
        'browser/extensions/extension_idle_api_unittest.cc',
        'browser/extensions/extension_info_map_unittest.cc',
        'browser/extensions/extension_menu_manager_unittest.cc',
        'browser/extensions/extension_omnibox_unittest.cc',
        'browser/extensions/extension_pref_value_map_unittest.cc',
        'browser/extensions/extension_prefs_unittest.cc',
        'browser/extensions/extension_process_manager_unittest.cc',
        'browser/extensions/extension_proxy_api_helpers_unittest.cc',
        'browser/extensions/extension_service_unittest.cc',
        'browser/extensions/extension_service_unittest.h',
        'browser/extensions/extension_special_storage_policy_unittest.cc',
        'browser/extensions/extension_sync_data_unittest.cc',
        'browser/extensions/extension_tts_api_controller_unittest.cc',
        'browser/extensions/extension_ui_unittest.cc',
        'browser/extensions/extension_updater_unittest.cc',
        'browser/extensions/extension_warning_set_unittest.cc',
        'browser/extensions/extension_webnavigation_unittest.cc',
        'browser/extensions/extension_webrequest_api_unittest.cc',
        'browser/extensions/extension_webrequest_time_tracker_unittest.cc',
        'browser/extensions/extensions_quota_service_unittest.cc',
        'browser/extensions/external_policy_extension_loader_unittest.cc',
        'browser/extensions/file_reader_unittest.cc',
        'browser/extensions/image_loading_tracker_unittest.cc',
        'browser/extensions/key_identifier_conversion_views_unittest.cc',
        'browser/extensions/network_delay_listener_unittest.cc',
        'browser/extensions/process_map_unittest.cc',
        'browser/extensions/sandboxed_extension_unpacker_unittest.cc',
        'browser/extensions/settings/settings_frontend_unittest.cc',
        'browser/extensions/settings/settings_leveldb_storage_unittest.cc',
        'browser/extensions/settings/settings_quota_unittest.cc',
        'browser/extensions/settings/settings_storage_cache_unittest.cc',
        'browser/extensions/settings/settings_storage_unittest.cc',
        'browser/extensions/settings/settings_storage_unittest.h',
        'browser/extensions/settings/settings_sync_unittest.cc',
        'browser/extensions/settings/settings_test_util.cc',
        'browser/extensions/settings/settings_test_util.h',
        'browser/extensions/settings/testing_settings_storage_unittest.cc',
        'browser/extensions/user_script_listener_unittest.cc',
        'browser/extensions/user_script_master_unittest.cc',
        'browser/external_protocol/external_protocol_handler_unittest.cc',
        'browser/favicon/favicon_handler_unittest.cc',
        'browser/first_run/first_run_unittest.cc',
        'browser/fragmentation_checker_unittest_win.cc',
        'browser/geolocation/chrome_geolocation_permission_context_unittest.cc',
        'browser/geolocation/geolocation_settings_state_unittest.cc',
        'browser/geolocation/wifi_data_provider_unittest_chromeos.cc',
        'browser/global_keyboard_shortcuts_mac_unittest.mm',
        'browser/google/google_update_settings_unittest.cc',
        'browser/google/google_url_tracker_unittest.cc',
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
        'browser/history/visit_tracker_unittest.cc',
        'browser/importer/firefox_importer_unittest.cc',
        'browser/importer/firefox_importer_unittest_messages_internal.h',
        'browser/importer/firefox_importer_unittest_utils.h',
        'browser/importer/firefox_importer_unittest_utils_mac.cc',
        'browser/importer/firefox_importer_utils_unittest.cc',
        'browser/importer/firefox_profile_lock_unittest.cc',
        'browser/importer/firefox_proxy_settings_unittest.cc',
        'browser/importer/importer_unittest.cc',
        'browser/importer/safari_importer_unittest.mm',
        'browser/importer/toolbar_importer_unittest.cc',
        'browser/instant/promo_counter_unittest.cc',
        'browser/intents/register_intent_handler_infobar_delegate_unittest.cc',
        'browser/intents/web_intents_registry_unittest.cc',
        'browser/internal_auth_unittest.cc',
        'browser/language_usage_metrics_unittest.cc',
        'browser/mac/keystone_glue_unittest.mm',
        'browser/media/media_internals_unittest.cc',
        'browser/metrics/metrics_log_unittest.cc',
        'browser/metrics/metrics_log_serializer_unittest.cc',
        'browser/metrics/metrics_response_unittest.cc',
        'browser/metrics/metrics_service_unittest.cc',
        'browser/metrics/thread_watcher_unittest.cc',
        'browser/mock_keychain_mac.cc',
        'browser/mock_keychain_mac.h',
        'browser/net/chrome_fraudulent_certificate_reporter_unittest.cc',
        'browser/net/chrome_net_log_unittest.cc',
        'browser/net/connection_tester_unittest.cc',
        'browser/net/gaia/gaia_oauth_fetcher_unittest.cc',
        'browser/net/gaia/token_service_unittest.cc',
        'browser/net/gaia/token_service_unittest.h',
        'browser/net/http_server_properties_manager_unittest.cc',
        'browser/net/load_timing_observer_unittest.cc',
        'browser/net/network_stats_unittest.cc',
        'browser/net/passive_log_collector_unittest.cc',
        'browser/net/predictor_unittest.cc',
        'browser/net/pref_proxy_config_tracker_impl_unittest.cc',
        'browser/net/quoted_printable_unittest.cc',
        'browser/net/sqlite_origin_bound_cert_store_unittest.cc',
        'browser/net/sqlite_persistent_cookie_store_unittest.cc',
        'browser/net/ssl_config_service_manager_pref_unittest.cc',
        'browser/net/url_fixer_upper_unittest.cc',
        'browser/net/url_info_unittest.cc',
        'browser/notifications/desktop_notification_service_unittest.cc',
        'browser/oom_priority_manager_unittest.cc',
        'browser/parsers/metadata_parser_filebase_unittest.cc',
        'browser/password_manager/encryptor_password_mac_unittest.cc',
        'browser/password_manager/encryptor_unittest.cc',
        'browser/password_manager/login_database_unittest.cc',
        'browser/password_manager/native_backend_gnome_x_unittest.cc',
        'browser/password_manager/native_backend_kwallet_x_unittest.cc',
        'browser/password_manager/password_form_data.cc',
        'browser/password_manager/password_form_manager_unittest.cc',
        'browser/password_manager/password_manager_unittest.cc',
        'browser/password_manager/password_store_default_unittest.cc',
        'browser/password_manager/password_store_mac_unittest.cc',
        'browser/password_manager/password_store_win_unittest.cc',
        'browser/password_manager/password_store_x_unittest.cc',
        'browser/plugin_prefs_unittest.cc',
        'browser/policy/asynchronous_policy_loader_unittest.cc',
        'browser/policy/asynchronous_policy_provider_unittest.cc',
        'browser/policy/asynchronous_policy_test_base.cc',
        'browser/policy/asynchronous_policy_test_base.h',
        'browser/policy/cloud_policy_controller_unittest.cc',
        'browser/policy/cloud_policy_provider_unittest.cc',
        'browser/policy/cloud_policy_subsystem_unittest.cc',
        'browser/policy/config_dir_policy_provider_unittest.cc',
        'browser/policy/configuration_policy_handler_chromeos_unittest.cc',
        'browser/policy/configuration_policy_pref_store_unittest.cc',
        'browser/policy/configuration_policy_provider_mac_unittest.cc',
        'browser/policy/configuration_policy_provider_test.cc',
        'browser/policy/configuration_policy_provider_test.h',
        'browser/policy/configuration_policy_provider_win_unittest.cc',
        'browser/policy/configuration_policy_reader_unittest.cc',
        'browser/policy/device_management_backend_mock.cc',
        'browser/policy/device_management_backend_mock.h',
        'browser/policy/device_management_service_unittest.cc',
        'browser/policy/device_policy_cache_unittest.cc',
        'browser/policy/device_token_fetcher_unittest.cc',
        'browser/policy/enterprise_install_attributes_unittest.cc',
        'browser/policy/file_based_policy_provider_unittest.cc',
        'browser/policy/logging_work_scheduler.cc',
        'browser/policy/logging_work_scheduler.h',
        'browser/policy/logging_work_scheduler_unittest.cc',
        'browser/policy/mock_configuration_policy_provider.cc',
        'browser/policy/mock_configuration_policy_provider.h',
        'browser/policy/mock_configuration_policy_reader.cc',
        'browser/policy/mock_configuration_policy_reader.h',
        'browser/policy/mock_device_management_backend.cc',
        'browser/policy/mock_device_management_backend.h',
        'browser/policy/network_configuration_updater_unittest.cc',
        'browser/policy/policy_map_unittest.cc',
        'browser/policy/policy_path_parser_unittest.cc',
        'browser/policy/testing_cloud_policy_subsystem.cc',
        'browser/policy/testing_cloud_policy_subsystem.h',
        'browser/policy/testing_policy_url_fetcher_factory.cc',
        'browser/policy/testing_policy_url_fetcher_factory.h',
        'browser/policy/url_blacklist_manager_unittest.cc',
        'browser/policy/user_policy_cache_unittest.cc',
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
        'browser/prerender/prerender_manager_unittest.cc',
        'browser/prerender/prerender_tracker_unittest.cc',
        'browser/prerender/prerender_util_unittest.cc',
        'browser/printing/cloud_print/cloud_print_proxy_service_unittest.cc',
        'browser/printing/cloud_print/cloud_print_setup_source_unittest.cc',
        'browser/printing/print_dialog_cloud_unittest.cc',
        'browser/printing/print_job_unittest.cc',
        'browser/printing/print_preview_tab_controller_unittest.cc',
        'browser/process_info_snapshot_mac_unittest.cc',
        'browser/process_singleton_mac_unittest.cc',
        'browser/profiles/avatar_menu_model_unittest.cc',
        'browser/profiles/off_the_record_profile_impl_unittest.cc',
        'browser/profiles/profile_dependency_manager_unittest.cc',
        'browser/profiles/profile_info_cache_unittest.cc',
        'browser/profiles/profile_manager_unittest.cc',
        'browser/renderer_host/web_cache_manager_unittest.cc',
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
        'browser/sessions/session_utils_unittest.cc',
        'browser/shell_integration_unittest.cc',
        'browser/speech/speech_input_bubble_controller_unittest.cc',
        'browser/spellchecker/spellchecker_platform_engine_unittest.cc',
        'browser/spellchecker/spellcheck_profile_unittest.cc',
        'browser/status_icons/status_icon_unittest.cc',
        'browser/status_icons/status_tray_unittest.cc',
        'browser/sync/abstract_profile_sync_service_test.cc',
        'browser/sync/abstract_profile_sync_service_test.h',
        'browser/sync/backend_migrator_unittest.cc',
        'browser/sync/internal_api/read_node_mock.cc',
        'browser/sync/internal_api/read_node_mock.h',
        'browser/sync/glue/app_notification_data_type_controller_unittest.cc',
        'browser/sync/glue/autofill_data_type_controller_unittest.cc',
        'browser/sync/glue/bookmark_data_type_controller_unittest.cc',
        'browser/sync/glue/browser_thread_model_worker_unittest.cc',
        'browser/sync/glue/change_processor_mock.cc',
        'browser/sync/glue/change_processor_mock.h',
        'browser/sync/glue/data_type_controller_mock.cc',
        'browser/sync/glue/data_type_controller_mock.h',
        'browser/sync/glue/data_type_manager_impl_unittest.cc',
        'browser/sync/glue/data_type_manager_mock.cc',
        'browser/sync/glue/data_type_manager_mock.h',
        'browser/sync/glue/extension_data_type_controller_unittest.cc',
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
        'browser/sync/glue/preference_data_type_controller_unittest.cc',
        'browser/sync/glue/search_engine_data_type_controller_unittest.cc',
        'browser/sync/glue/session_model_associator_unittest.cc',
        'browser/sync/glue/shared_change_processor_mock.cc',
        'browser/sync/glue/shared_change_processor_mock.h',
        'browser/sync/glue/sync_backend_host_mock.cc',
        'browser/sync/glue/sync_backend_host_mock.h',
        'browser/sync/glue/sync_backend_host_unittest.cc',
        'browser/sync/glue/sync_backend_registrar_unittest.cc',
        'browser/sync/glue/synced_session_tracker_unittest.cc',
        'browser/sync/glue/theme_data_type_controller_unittest.cc',
        'browser/sync/glue/theme_util_unittest.cc',
        'browser/sync/glue/typed_url_model_associator_unittest.cc',
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
        'browser/sync/signin_manager_unittest.cc',
        'browser/sync/sync_global_error_unittest.cc',
        'browser/sync/sync_prefs_unittest.cc',
        'browser/sync/sync_setup_wizard_unittest.cc',
        'browser/sync/sync_ui_util_mac_unittest.mm',
        'browser/sync/sync_ui_util_unittest.cc',
        'browser/sync/test_profile_sync_service.cc',
        'browser/sync/test_profile_sync_service.h',
        'browser/sync/test/test_http_bridge_factory.cc',
        'browser/sync/test/test_http_bridge_factory.h',
        'browser/sync/util/cryptographer_unittest.cc',
        'browser/sync/util/nigori_unittest.cc',
        'browser/tab_contents/render_view_context_menu_unittest.cc',
        'browser/tab_contents/thumbnail_generator_unittest.cc',
        'browser/tab_contents/web_contents_unittest.cc',
        'browser/tabs/pinned_tab_codec_unittest.cc',
        'browser/tabs/pinned_tab_service_unittest.cc',
        'browser/tabs/pinned_tab_test_utils.cc',
        'browser/tabs/tab_strip_model_unittest.cc',
        'browser/tabs/tab_strip_selection_model_unittest.cc',
        'browser/task_manager/task_manager_unittest.cc',
        'browser/themes/browser_theme_pack_unittest.cc',
        'browser/themes/theme_service_unittest.cc',
        'browser/ui/browser_list_unittest.cc',
        # It is safe to list */cocoa/* files in the "common" file list
        # without an explicit exclusion since gyp is smart enough to
        # exclude them from non-Mac builds.
        'browser/ui/cocoa/about_ipc_controller_unittest.mm',
        'browser/ui/cocoa/about_window_controller_unittest.mm',
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
        'browser/ui/cocoa/clickhold_button_cell_unittest.mm',
        'browser/ui/cocoa/cocoa_profile_test.h',
        'browser/ui/cocoa/cocoa_profile_test.mm',
        'browser/ui/cocoa/cocoa_test_helper.h',
        'browser/ui/cocoa/cocoa_test_helper.mm',
        'browser/ui/cocoa/command_observer_bridge_unittest.mm',
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
        'browser/ui/cocoa/focus_tracker_unittest.mm',
        'browser/ui/cocoa/framed_browser_window_unittest.mm',
        'browser/ui/cocoa/fullscreen_window_unittest.mm',
        'browser/ui/cocoa/fullscreen_exit_bubble_controller_unittest.mm',
        'browser/ui/cocoa/gradient_button_cell_unittest.mm',
        'browser/ui/cocoa/history_menu_bridge_unittest.mm',
        'browser/ui/cocoa/history_menu_cocoa_controller_unittest.mm',
        'browser/ui/cocoa/hover_image_button_unittest.mm',
        'browser/ui/cocoa/html_dialog_window_controller_unittest.mm',
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
        'browser/ui/cocoa/location_bar/instant_opt_in_controller_unittest.mm',
        'browser/ui/cocoa/location_bar/instant_opt_in_view_unittest.mm',
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
        'browser/ui/cocoa/page_info_bubble_controller_unittest.mm',
        'browser/ui/cocoa/profile_menu_controller_unittest.mm',
        'browser/ui/cocoa/run_loop_testing.h',
        'browser/ui/cocoa/run_loop_testing.mm',
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
        'browser/ui/cocoa/web_intent_bubble_controller_unittest.mm',
        'browser/ui/cocoa/window_size_autosaver_unittest.mm',
        'browser/ui/cocoa/wrench_menu/menu_tracked_button_unittest.mm',
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
        'browser/ui/gtk/reload_button_gtk_unittest.cc',
        'browser/ui/gtk/status_icons/status_tray_gtk_unittest.cc',
        'browser/ui/gtk/tabs/tab_renderer_gtk_unittest.cc',
        'browser/ui/intents/web_intent_picker_controller_unittest.cc',
        'browser/ui/intents/web_intents_model_unittest.cc',
        'browser/ui/login/login_prompt_unittest.cc',
        'browser/ui/omnibox/omnibox_view_unittest.cc',
        'browser/ui/panels/auto_hiding_desktop_bar_win_unittest.cc',
        'browser/ui/panels/panel_browser_window_cocoa_unittest.mm',
        'browser/ui/search_engines/keyword_editor_controller_unittest.cc',
        'browser/ui/select_file_dialog_unittest.cc',
        'browser/ui/tabs/dock_info_unittest.cc',
        'browser/ui/tabs/tab_menu_model_unittest.cc',
        'browser/ui/tests/ui_gfx_image_unittest.cc',
        'browser/ui/tests/ui_gfx_image_unittest.mm',
        'browser/ui/toolbar/back_forward_menu_model_unittest.cc',
        'browser/ui/toolbar/encoding_menu_controller_unittest.cc',
        'browser/ui/toolbar/toolbar_model_unittest.cc',
        'browser/ui/toolbar/wrench_menu_model_unittest.cc',
        'browser/ui/touch/tabs/touch_tab_strip_unittest.cc',
        'browser/ui/views/accessibility_event_router_views_unittest.cc',
        'browser/ui/views/bookmarks/bookmark_context_menu_test.cc',
        'browser/ui/views/bookmarks/bookmark_editor_view_unittest.cc',
        'browser/ui/views/bubble/border_contents_unittest.cc',
        'browser/ui/views/extensions/browser_action_drag_data_unittest.cc',
        'browser/ui/views/first_run_bubble_unittest.cc',
        'browser/ui/views/first_run_search_engine_view_unittest.cc',
        'browser/ui/views/generic_info_view_unittest.cc',
        'browser/ui/views/reload_button_unittest.cc',
        'browser/ui/views/select_file_dialog_extension_unittest.cc',
        'browser/ui/views/select_file_dialog_win_unittest.cc',
        'browser/ui/views/status_icons/status_tray_win_unittest.cc',
        'browser/ui/views/tabs/base_tab_strip_test_fixture.h',
        'browser/ui/views/tabs/fake_base_tab_strip_controller.cc',
        'browser/ui/views/tabs/fake_base_tab_strip_controller.h',
        'browser/ui/views/tabs/tab_strip_unittest.cc',
        'browser/ui/webui/chrome_web_ui_data_source_unittest.cc',
        'browser/ui/webui/chromeos/imageburner/imageburner_utils_unittest.cc',
        'browser/ui/webui/html_dialog_tab_contents_delegate_unittest.cc',
        'browser/ui/webui/options/chromeos/virtual_keyboard_manager_handler_unittest.cc',
        'browser/ui/webui/options/language_options_handler_unittest.cc',
        'browser/ui/webui/print_preview_handler_unittest.cc',
        'browser/ui/webui/print_preview_ui_unittest.cc',
        'browser/ui/webui/sync_internals_ui_unittest.cc',
        'browser/ui/webui/theme_source_unittest.cc',
        'browser/ui/webui/web_ui_unittest.cc',
        'browser/ui/window_sizer_unittest.cc',
        'browser/ui/window_snapshot/window_snapshot_mac_unittest.mm',
        'browser/user_style_sheet_watcher_unittest.cc',
        'browser/visitedlink/visitedlink_unittest.cc',
        'browser/web_applications/web_app_unittest.cc',
        'browser/web_resource/promo_resource_service_unittest.cc',
        'browser/webdata/autofill_entry_unittest.cc',
        'browser/webdata/autofill_profile_syncable_service_unittest.cc',
        'browser/webdata/autofill_table_unittest.cc',
        'browser/webdata/keyword_table_unittest.cc',
        'browser/webdata/logins_table_unittest.cc',
        'browser/webdata/token_service_table_unittest.cc',
        'browser/webdata/web_apps_table_unittest.cc',
        'browser/webdata/web_data_service_test_util.h',
        'browser/webdata/web_data_service_unittest.cc',
        'browser/webdata/web_database_migration_unittest.cc',
        'browser/webdata/web_intents_table_unittest.cc',
        'common/bzip2_unittest.cc',
        'common/child_process_logging_mac_unittest.mm',
        'common/chrome_paths_unittest.cc',
        'common/common_param_traits_unittest.cc',
        'common/content_settings_helper_unittest.cc',
        'common/content_settings_pattern_parser_unittest.cc',
        'common/content_settings_pattern_unittest.cc',
        'common/extensions/extension_action_unittest.cc',
        'common/extensions/extension_file_util_unittest.cc',
        'common/extensions/extension_icon_set_unittest.cc',
        'common/extensions/extension_l10n_util_unittest.cc',
        'common/extensions/extension_localization_peer_unittest.cc',
        'common/extensions/extension_manifests_unittest.cc',
        'common/extensions/extension_message_bundle_unittest.cc',
        'common/extensions/extension_permission_set_unittest.cc',
        'common/extensions/extension_resource_unittest.cc',
        'common/extensions/extension_set_unittest.cc',
        'common/extensions/extension_test_util.h',
        'common/extensions/extension_test_util.cc',
        'common/extensions/extension_unittest.cc',
        'common/extensions/extension_unpacker_unittest.cc',
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
        'common/mac/objc_method_swizzle_unittest.mm',
        'common/mac/objc_zombie_unittest.mm',
        'common/metrics_log_manager_unittest.cc',
        'common/multi_process_lock_unittest.cc',
        'common/net/gaia/gaia_auth_fetcher_unittest.cc',
        'common/net/gaia/gaia_auth_fetcher_unittest.h',
        'common/net/gaia/gaia_authenticator_unittest.cc',
        'common/net/gaia/gaia_oauth_client_unittest.cc',
        'common/net/gaia/google_service_auth_error_unittest.cc',
        'common/net/gaia/oauth_request_signer_unittest.cc',
        'common/random_unittest.cc',
        'common/service_process_util_unittest.cc',
        'common/string_ordinal_unittest.cc',
        'common/switch_utils_unittest.cc',
        'common/thumbnail_score_unittest.cc',
        'common/time_format_unittest.cc',
        'common/web_apps_unittest.cc',
        'common/worker_thread_ticker_unittest.cc',
        'common/zip_reader_unittest.cc',
        'common/zip_unittest.cc',
        'renderer/extensions/chrome_v8_context_set_unittest.cc',
        'renderer/extensions/extension_api_json_validity_unittest.cc',
        'renderer/extensions/json_schema_unittest.cc',
        'renderer/net/predictor_queue_unittest.cc',
        'renderer/net/renderer_predictor_unittest.cc',
        'renderer/plugin_uma_unittest.cc',
        'renderer/renderer_about_handler_unittest.cc',
        'renderer/safe_browsing/features_unittest.cc',
        'renderer/safe_browsing/murmurhash3_util_unittest.cc',
        'renderer/safe_browsing/phishing_term_feature_extractor_unittest.cc',
        'renderer/safe_browsing/phishing_url_feature_extractor_unittest.cc',
        'renderer/safe_browsing/scorer_unittest.cc',
        'renderer/spellchecker/spellcheck_provider_unittest.cc',
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
        'tools/convert_dict/convert_dict_unittest.cc',
        '../content/browser/renderer_host/render_widget_host_unittest.cc',
        '../content/browser/renderer_host/text_input_client_mac_unittest.mm',
        '../content/browser/tab_contents/navigation_controller_unittest.cc',
        '../content/browser/tab_contents/render_view_host_manager_unittest.cc',
        '../content/common/sandbox_mac_diraccess_unittest.mm',
        '../content/common/sandbox_mac_fontloading_unittest.mm',
        '../content/common/sandbox_mac_unittest_helper.h',
        '../content/common/sandbox_mac_unittest_helper.mm',
        '../content/common/sandbox_mac_system_access_unittest.mm',
        '../testing/gtest_mac_unittest.mm',
        '../third_party/cld/encodings/compact_lang_det/compact_lang_det_unittest_small.cc',
        '../ui/views/test/test_views_delegate.cc',
        '../ui/views/test/test_views_delegate.h',
        '../ui/views/test/views_test_base.cc',
        '../ui/views/test/views_test_base.h',
        '../webkit/fileapi/file_system_dir_url_request_job_unittest.cc',
        '../webkit/fileapi/file_system_operation_write_unittest.cc',
        '../webkit/fileapi/file_system_url_request_job_unittest.cc',
        '../webkit/fileapi/file_writer_delegate_unittest.cc',
        '../webkit/fileapi/file_system_test_helper.cc',
        '../webkit/fileapi/file_system_test_helper.h',
        '../webkit/quota/mock_storage_client.cc',
        '../webkit/quota/mock_storage_client.h',
      ],
      'rules': [
        {
          'rule_name': 'copyjs',
          'extension': 'js',
          'msvs_external_rule': 1,
          'inputs': [
            '../build/cp.py',
          ],
          'outputs': [
            '<(PRODUCT_DIR)/test_data/chrome/<(RULE_INPUT_DIRNAME)/<(RULE_INPUT_ROOT).<(_extension)',
          ],
          'action': [
            'python',
            '<@(_inputs)',
            '<(RULE_INPUT_PATH)',
            '<@(_outputs)',
          ],
        },
        {
          'rule_name': 'js2unit',
          'extension': 'gtestjs',
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
            '<(PRODUCT_DIR)/test_data/chrome/<(RULE_INPUT_DIRNAME)/<(RULE_INPUT_ROOT).<(_extension)',
          ],
          'process_outputs_as_sources': 1,
          'action': [
            'python',
            '<@(_inputs)',
            'unit',
            '<(RULE_INPUT_PATH)',
            'chrome/<(RULE_INPUT_DIRNAME)/<(RULE_INPUT_ROOT).<(_extension)',
            '<@(_outputs)',
          ],
        },
      ],
      'conditions': [
        ['target_arch!="arm"', {
          'dependencies': [
            # build time dependency.
            '../v8/tools/gyp/v8.gyp:v8_shell#host',
          ],
        }],
        ['p2p_apis==1', {
          'sources': [
            '../content/browser/renderer_host/p2p/socket_host_test_utils.h',
            '../content/browser/renderer_host/p2p/socket_host_tcp_unittest.cc',
            '../content/browser/renderer_host/p2p/socket_host_tcp_server_unittest.cc',
            '../content/browser/renderer_host/p2p/socket_host_udp_unittest.cc',
            '../content/renderer/p2p/p2p_transport_impl_unittest.cc',
          ],
        }],
        ['use_aura==1', {
          'sources/': [
            ['exclude', '^browser/automation/automation_provider_unittest.cc'],
          ],
        }],
        ['touchui==0', {
          'sources/': [
            ['exclude', '^browser/ui/touch/'],
            ['exclude', '^browser/ui/webui/chromeos/login/'],
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
        ['chromeos==1', {
          'sources/': [
            ['exclude', '^browser/password_manager/native_backend_gnome_x_unittest.cc'],
            ['exclude', '^browser/password_manager/native_backend_kwallet_x_unittest.cc'],
            ['exclude', '^browser/safe_browsing/download_protection_service_unittest.cc' ],
            ['exclude', '^../content/browser/geolocation/wifi_data_provider_linux_unittest.cc'],
            # TODO(thestig) Enable PrintPreviewUI tests on CrOS when
            # print preview is enabled on CrOS.
            ['exclude', '^browser/ui/webui/print_preview_ui_unittest.cc'],
          ],
          'conditions': [
            ['use_aura==1', {
              'sources!': [
                'browser/chromeos/notifications/desktop_notifications_unittest.cc',
              ],
            }],
          ],
        }, { # else: chromeos == 0
          'sources/': [
            ['exclude', '^browser/chromeos/'],
            ['exclude', '^browser/oom_priority_manager_unittest.cc'],
            ['exclude', '^browser/policy/configuration_policy_handler_chromeos_unittest.cc' ],
            ['exclude', '^browser/policy/device_policy_cache_unittest.cc'],
            ['exclude', '^browser/policy/enterprise_install_attributes_unittest.cc' ],
            ['exclude', '^browser/policy/network_configuration_updater_unittest.cc' ],
            ['exclude', '^browser/ui/webui/chromeos/imageburner/'],
            ['exclude', '^browser/ui/webui/chromeos/login'],
            ['exclude', '^browser/ui/webui/options/chromeos/'],
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
            'browser/chromeos/input_method/input_method.gyp:mozc_commands_proto',
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
            '../content/content.gyp:closure_blocks_leopard_compat',
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
            '../third_party/hunspell/google/hunspell_tests.cc',
          ],
          # TODO(mark): We really want this for all non-static library targets,
          # but when we tried to pull it up to the common.gypi level, it broke
          # other things like the ui, startup, and page_cycler tests. *shrug*
          'xcode_settings': {'OTHER_LDFLAGS': ['-Wl,-ObjC']},

          # libwebcore.a is so large that ld may not have a sufficiently large
          # "hole" in its address space into which it can be mmaped by the
          # time it reaches this library. As of May 10, 2010, libwebcore.a is
          # about 1GB in some builds. In the Mac OS X 10.5 toolchain, using
          # Xcode 3.1, ld is only a 32-bit executable, and address space
          # exhaustion is the result, with ld failing and producing
          # the message:
          # ld: in .../libwebcore.a, can't map file, errno=12
          #
          # As a workaround, ensure that libwebcore.a appears to ld first when
          # linking unit_tests. This allows the library to be mmapped when
          # ld's address space is "wide open." Other libraries are small
          # enough that they'll be able to "squeeze" into the remaining holes.
          # The Mac linker isn't so sensitive that moving this library to the
          # front of the list will cause problems.
          #
          # Enough pluses to make get this target prepended to the target's
          # list of dependencies.
          'dependencies+++': [
            '../third_party/WebKit/Source/WebCore/WebCore.gyp/WebCore.gyp:webcore',
          ],
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
            '<(SHARED_INTERMEDIATE_DIR)/chrome/renderer_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/theme_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/theme_resources_standard.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome_version/other_version.rc',
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
            'browser/rlz/rlz_unittest.cc',
            'browser/search_engines/template_url_scraper_unittest.cc',
            'browser/ui/views/bookmarks/bookmark_editor_view_unittest.cc',
            'browser/ui/views/extensions/browser_action_drag_data_unittest.cc',
            'browser/ui/views/first_run_search_engine_view_unittest.cc',
            'browser/ui/views/generic_info_view_unittest.cc',
            'test/data/resource.rc',
          ],
        }],
        ['toolkit_views==1', {
          'dependencies': [
            '../views/views.gyp:views',
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
      # Executable that runs each browser test in a new process.
      'target_name': 'browser_tests',
      'type': 'executable',
      'msvs_cygwin_shell': 0,
      'msvs_cygwin_dirs': ['<(DEPTH)/third_party/cygwin'],
      'dependencies': [
        'browser',
        'browser/sync/protocol/sync_proto.gyp:sync_proto',
        'chrome',
        'chrome_resources.gyp:chrome_resources',
        'chrome_resources.gyp:chrome_strings',
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
        '../third_party/WebKit/Source/WebKit/chromium/WebKit.gyp:copy_TestNetscapePlugIn',
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
        'browser/accessibility/renderer_accessibility_browsertest.cc',
        'browser/autocomplete/autocomplete_browsertest.cc',
        'browser/autofill/form_structure_browsertest.cc',
        'browser/automation/automation_tab_helper_browsertest.cc',
        'browser/bookmarks/bookmark_extension_apitest.cc',
        'browser/bookmarks/bookmark_manager_extension_apitest.cc',
        'browser/browsing_data_database_helper_browsertest.cc',
        'browser/browsing_data_helper_browsertest.h',
        'browser/browsing_data_indexed_db_helper_browsertest.cc',
        'browser/browsing_data_local_storage_helper_browsertest.cc',
        'browser/chromeos/brightness_bubble_browsertest.cc',
        'browser/chromeos/cros/cros_in_process_browser_test.cc',
        'browser/chromeos/cros/cros_in_process_browser_test.h',
        'browser/chromeos/cros/cros_mock.cc',
        'browser/chromeos/cros/cros_mock.h',
        'browser/chromeos/cros/mock_power_library.cc',
        'browser/chromeos/cros/mock_power_library.h',
        'browser/chromeos/cros/mock_screen_lock_library.cc',
        'browser/chromeos/cros/mock_screen_lock_library.h',
        'browser/chromeos/cros/mock_update_library.cc',
        'browser/chromeos/cros/mock_update_library.h',
        'browser/chromeos/disks/mock_disk_mount_manager.cc',
        'browser/chromeos/disks/mock_disk_mount_manager.h',
        'browser/chromeos/extensions/file_browser_notifications_browsertest.cc',
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
        'browser/chromeos/login/screen_locker_browsertest.cc',
        'browser/chromeos/login/screen_locker_tester.cc',
        'browser/chromeos/login/screen_locker_tester.h',
        'browser/chromeos/login/update_screen_browsertest.cc',
        'browser/chromeos/login/wizard_controller_browsertest.cc',
        'browser/chromeos/login/wizard_in_process_browser_test.cc',
        'browser/chromeos/login/wizard_in_process_browser_test.h',
        'browser/chromeos/media/media_player_browsertest.cc',
        'browser/chromeos/notifications/notification_browsertest.cc',
        'browser/chromeos/panels/panel_browsertest.cc',
        'browser/chromeos/status/accessibility_menu_button_browsertest.cc',
        'browser/chromeos/status/caps_lock_menu_button_browsertest.cc',
        'browser/chromeos/status/clock_menu_button_browsertest.cc',
        'browser/chromeos/status/input_method_menu_button_browsertest.cc',
        'browser/chromeos/status/power_menu_button_browsertest.cc',
        'browser/chromeos/status/status_area_view_browsertest.cc',
        'browser/chromeos/setting_level_bubble_browsertest.cc',
        'browser/chromeos/tab_closeable_state_watcher_browsertest.cc',
        'browser/chromeos/volume_bubble_browsertest.cc',
        'browser/content_settings/content_settings_browsertest.cc',
        'browser/crash_recovery_browsertest.cc',
        'browser/custom_handlers/protocol_handler_registry_browsertest.cc',
        'browser/download/download_browsertest.cc',
        'browser/download/download_extension_apitest.cc',
        'browser/download/download_test_observer.cc',
        'browser/download/save_page_browsertest.cc',
        'browser/errorpage_browsertest.cc',
        'browser/extensions/alert_apitest.cc',
        'browser/extensions/all_urls_apitest.cc',
        'browser/extensions/app_background_page_apitest.cc',
        'browser/extensions/app_process_apitest.cc',
        'browser/extensions/autoupdate_interceptor.cc',
        'browser/extensions/autoupdate_interceptor.h',
        'browser/extensions/browser_action_apitest.cc',
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
        'browser/extensions/extension_browsertest.cc',
        'browser/extensions/extension_browsertest.h',
        'browser/extensions/extension_browsertests_misc.cc',
        'browser/extensions/extension_chrome_auth_private_apitest.cc',
        'browser/extensions/extension_clear_apitest.cc',
        'browser/extensions/extension_clipboard_apitest.cc',
        'browser/extensions/extension_content_settings_apitest.cc',
        'browser/extensions/extension_context_menu_apitest.cc',
        'browser/extensions/extension_context_menu_browsertest.cc',
        'browser/extensions/extension_cookies_apitest.cc',
        'browser/extensions/extension_crash_recovery_browsertest.cc',
        'browser/extensions/extension_debugger_apitest.cc',
        'browser/extensions/extension_decode_jpeg_apitest.cc',
        'browser/extensions/extension_devtools_browsertest.cc',
        'browser/extensions/extension_devtools_browsertest.h',
        'browser/extensions/extension_devtools_browsertests.cc',
        'browser/extensions/extension_dom_clipboard_apitest.cc',
        'browser/extensions/extension_file_browser_private_apitest.cc',
        'browser/extensions/extension_fileapi_apitest.cc',
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
        'browser/extensions/extension_input_ime_apitest.cc',
        'browser/extensions/extension_input_method_apitest.cc',
        'browser/extensions/extension_input_ui_apitest.cc',
        'browser/extensions/extension_install_ui_browsertest.cc',
        'browser/extensions/extension_javascript_url_apitest.cc',
        'browser/extensions/extension_local_filesystem_apitest.cc',
        'browser/extensions/extension_management_api_browsertest.cc',
        'browser/extensions/extension_management_apitest.cc',
        'browser/extensions/extension_management_browsertest.cc',
        'browser/extensions/extension_messages_apitest.cc',
        'browser/extensions/extension_messages_browsertest.cc',
        'browser/extensions/extension_metrics_apitest.cc',
        'browser/extensions/extension_module_apitest.cc',
        'browser/extensions/extension_nacl_browsertest.cc',
        'browser/extensions/extension_omnibox_apitest.cc',
        'browser/extensions/extension_override_apitest.cc',
        'browser/extensions/extension_preference_apitest.cc',
        'browser/extensions/extension_processes_apitest.cc',
        'browser/extensions/extension_proxy_apitest.cc',
        'browser/extensions/extension_resource_request_policy_apitest.cc',
        'browser/extensions/extension_save_page_apitest.cc',
        'browser/extensions/extension_sidebar_apitest.cc',
        'browser/extensions/extension_startup_browsertest.cc',
        'browser/extensions/extension_storage_apitest.cc',
        'browser/extensions/extension_tabs_apitest.cc',
        'browser/extensions/extension_tabs_test.cc',
        'browser/extensions/extension_test_message_listener.cc',
        'browser/extensions/extension_test_message_listener.h',
        'browser/extensions/extension_toolbar_model_browsertest.cc',
        'browser/extensions/extension_tts_apitest.cc',
        'browser/extensions/extension_url_rewrite_browsertest.cc',
        'browser/extensions/extension_web_socket_proxy_private_apitest.cc',
        'browser/extensions/extension_webglbackground_apitest.cc',
        'browser/extensions/extension_webnavigation_apitest.cc',
        'browser/extensions/extension_webrequest_apitest.cc',
        'browser/extensions/extension_websocket_apitest.cc',
        'browser/extensions/extension_webstore_private_apitest.cc',
        'browser/extensions/isolated_app_browsertest.cc',
        'browser/extensions/lazy_background_page_apitest.cc',
        'browser/extensions/notifications_apitest.cc',
        'browser/extensions/page_action_apitest.cc',
        'browser/extensions/permissions_apitest.cc',
        'browser/extensions/settings/settings_apitest.cc',
        'browser/extensions/stubs_apitest.cc',
        'browser/extensions/webstore_inline_install_browsertest.cc',
        'browser/extensions/window_open_apitest.cc',
        'browser/first_run/first_run_browsertest.cc',
        'browser/geolocation/access_token_store_browsertest.cc',
        'browser/geolocation/geolocation_browsertest.cc',
        'browser/gpu_browsertest.cc',
        'browser/history/history_browsertest.cc',
        'browser/history/history_extension_apitest.cc',
        'browser/history/top_sites_extension_test.cc',
        'browser/infobars/infobar_extension_apitest.cc',
        'browser/importer/toolbar_importer_utils_browsertest.cc',
        'browser/magic_iframe_browsertest.cc',
        'browser/net/cookie_policy_browsertest.cc',
        'browser/net/ftp_browsertest.cc',
        'browser/notifications/desktop_notifications_unittest.cc',
        'browser/notifications/desktop_notifications_unittest.h',
        'browser/oom_priority_manager_browsertest.cc',
        'browser/policy/device_management_backend_mock.cc',
        'browser/policy/device_management_backend_mock.h',
        'browser/policy/device_management_service_browsertest.cc',
        'browser/policy/enterprise_metrics_browsertest.cc',
        'browser/policy/enterprise_metrics_enrollment_browsertest.cc',
        'browser/popup_blocker_browsertest.cc',
        'browser/prerender/prerender_browsertest.cc',
        'browser/printing/print_dialog_cloud_uitest.cc',
        'browser/printing/print_preview_tab_controller_browsertest.cc',
        'browser/profiles/profile_manager_browsertest.cc',
        'browser/rlz/rlz_extension_apitest.cc',
        'browser/renderer_host/render_process_host_chrome_browsertest.cc',
        'browser/renderer_host/web_cache_manager_browsertest.cc',
        'browser/safe_browsing/safe_browsing_blocking_page_test.cc',
        'browser/safe_browsing/safe_browsing_service_browsertest.cc',
        'browser/service/service_process_control_browsertest.cc',
        'browser/sessions/session_restore_browsertest.cc',
        'browser/sessions/tab_restore_service_browsertest.cc',
        'browser/speech/speech_input_bubble_browsertest.cc',
        'browser/speech/speech_input_extension_apitest.cc',
        'browser/spellchecker/spellcheck_host_browsertest.cc',
        'browser/ssl/ssl_browser_tests.cc',
        'browser/tab_first_render_watcher_browsertest.cc',
        'browser/task_manager/task_manager_browsertest.cc',
        'browser/task_manager/task_manager_browsertest_util.cc',
        'browser/task_manager/task_manager_browsertest_util.h',
        'browser/task_manager/task_manager_notification_browsertest.cc',
        'browser/translate/translate_manager_browsertest.cc',
        'browser/ui/browser_browsertest.cc',
        'browser/ui/browser_close_browsertest.cc',
        'browser/ui/browser_init_browsertest.cc',
        'browser/ui/browser_navigator_browsertest.cc',
        'browser/ui/browser_navigator_browsertest.h',
        'browser/ui/browser_navigator_browsertest_chromeos.cc',
        'browser/ui/cocoa/content_settings/content_setting_bubble_cocoa_unittest.mm',
        'browser/ui/cocoa/view_id_util_browsertest.mm',
        'browser/ui/cocoa/applescript/browsercrapplication+applescript_test.mm',
        'browser/ui/cocoa/applescript/window_applescript_test.mm',
        'browser/ui/find_bar/find_bar_host_browsertest.cc',
        'browser/ui/global_error_service_browsertest.cc',
        'browser/ui/gtk/view_id_util_browsertest.cc',
        'browser/ui/intents/web_intent_picker_controller_browsertest.cc',
        'browser/ui/login/login_prompt_browsertest.cc',
        'browser/ui/panels/panel_app_browsertest.cc',
        'browser/ui/panels/panel_browser_view_browsertest.cc',
        'browser/ui/views/browser_actions_container_browsertest.cc',
        'browser/ui/views/dom_view_browsertest.cc',
        'browser/ui/views/html_dialog_view_browsertest.cc',
        'browser/ui/views/select_file_dialog_extension_browsertest.cc',
        'browser/ui/views/status_icons/status_tray_chromeos_browsertest.cc',
        'browser/ui/webui/chrome_url_data_manager_browsertest.cc',
        'browser/ui/webui/ntp/most_visited_browsertest.cc',
        'browser/ui/webui/test_chrome_web_ui_factory_browsertest.cc',
        'browser/ui/webui/bidi_checker_web_ui_test.cc',
        'browser/ui/webui/bidi_checker_web_ui_test.h',
        'browser/ui/webui/net_internals_ui_browsertest.cc',
        'browser/ui/webui/ntp/new_tab_ui_browsertest.cc',
        'browser/ui/webui/options/advanced_options_browsertest.js',
        'browser/ui/webui/options/autofill_options_browsertest.js',
        'browser/ui/webui/options/browser_options_browsertest.js',
        'browser/ui/webui/options/certificate_manager_browsertest.js',
        'browser/ui/webui/options/content_options_browsertest.js',
        'browser/ui/webui/options/content_settings_exception_area_browsertest.js',
        'browser/ui/webui/options/cookies_view_browsertest.js',
        'browser/ui/webui/options/font_settings_browsertest.js',
        'browser/ui/webui/options/language_options_browsertest.js',
        'browser/ui/webui/options/options_browsertest.js',
        'browser/ui/webui/options/password_manager_browsertest.js',
        'browser/ui/webui/options/personal_options_browsertest.js',
        'browser/ui/webui/options/search_engine_manager_browsertest.js',
        'browser/ui/webui/sync_setup_browsertest.js',
        'browser/ui/webui/test_html_dialog_ui_delegate.cc',
        'browser/ui/webui/test_html_dialog_ui_delegate.h',
        'browser/ui/webui/web_ui_browsertest.cc',
        'browser/ui/webui/web_ui_browsertest.h',
        'browser/ui/webui/web_ui_test_handler.cc',
        'browser/ui/webui/web_ui_test_handler.h',
        'common/time_format_browsertest.cc',
        'renderer/autofill/autofill_browsertest.cc',
        'renderer/autofill/form_autocomplete_browsertest.cc',
        'renderer/autofill/form_autofill_browsertest.cc',
        'renderer/autofill/password_autofill_manager_browsertest.cc',
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
        'test/base/in_process_browser_test_browsertest.cc',
        'test/base/chrome_render_view_test.cc',
        'test/base/chrome_render_view_test.h',
        'test/base/chrome_test_launcher.cc',
        'test/data/webui/assertions.js',
        'test/data/webui/async_gen-inl.h',
        'test/data/webui/async_gen.js',
        'test/data/webui/certificate_viewer_dialog_test.js',
        'test/data/webui/certificate_viewer_ui_test-inl.h',
        'test/data/webui/chrome_send_browsertest.cc',
        'test/data/webui/chrome_send_browsertest.h',
        'test/data/webui/chrome_send_browsertest.js',
        'test/data/webui/hung_renderer_dialog_test.js',
        'test/data/webui/hung_renderer_dialog_ui_test-inl.h',
        'test/data/webui/mock4js_browsertest.js',
        'test/data/webui/ntp4.js',
        'test/data/webui/print_preview.js',
        'test/gpu/gpu_feature_browsertest.cc',
        # TODO(craig): Rename this and run from base_unittests when the test
        # is safe to run there. See http://crbug.com/78722 for details.
        '../base/files/file_path_watcher_browsertest.cc',
        '../content/app/startup_helper_win.cc',
        '../content/browser/child_process_security_policy_browsertest.cc',
        '../content/browser/device_orientation/device_orientation_browsertest.cc',
        '../content/browser/download/mhtml_generation_browsertest.cc',
        '../content/browser/file_system/file_system_browsertest.cc',
        '../content/browser/idbbindingutilities_browsertest.cc',
        '../content/browser/in_process_webkit/dom_storage_browsertest.cc',
        '../content/browser/in_process_webkit/indexed_db_browsertest.cc',
        '../content/browser/plugin_data_remover_impl_browsertest.cc',
        '../content/browser/plugin_service_browsertest.cc',
        '../content/browser/renderer_host/render_process_host_browsertest.cc',
        '../content/browser/renderer_host/render_process_host_browsertest.h',
        '../content/browser/renderer_host/render_view_host_browsertest.cc',
        '../content/browser/renderer_host/render_view_host_manager_browsertest.cc',
        '../content/browser/renderer_host/resource_dispatcher_host_browsertest.cc',
        '../content/browser/speech/speech_input_browsertest.cc',
	'../content/browser/webkit_browsertest.cc',
        '../content/renderer/render_view_browsertest.cc',
        '../content/renderer/render_view_browsertest_mac.mm',
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
        ['chromeos==0', {
          'sources/': [
            ['exclude', '^browser/chromeos'],
          ],
          'sources!': [
            'browser/chromeos/media/media_player_browsertest.cc',
            'browser/extensions/extension_file_browser_private_apitest.cc',
            'browser/extensions/extension_input_method_apitest.cc',
            'browser/oom_priority_manager_browsertest.cc',
            'browser/policy/enterprise_metrics_enrollment_browsertest.cc',
            'browser/ui/views/status_icons/status_tray_chromeos_browsertest.cc',
          ],
        }, { #else: OS == "chromeos"
          'sources!': [
            'browser/notifications/desktop_notifications_unittest.cc',
            'browser/service/service_process_control_browsertest.cc',
            # chromeos does not use cross-platform panels
            'browser/ui/panels/panel_app_browsertest.cc',
            'browser/ui/panels/panel_browser_view_browsertest.cc',
            'browser/ui/webui/print_preview.js',
          ],
          'conditions': [
            ['use_aura==1', {
              'sources/': [
                ['exclude', '^browser/chromeos/login/screen_locker_browsertest.cc'],
                ['exclude', '^browser/chromeos/login/screen_locker_tester.cc'],
                ['exclude', '^browser/chromeos/notifications/'],
                ['exclude', '^browser/ui/views/status_icons/status_tray_chromeos_browsertest.cc'],
              ],
            }],
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
        ['toolkit_views==0', {
          'sources!': [
            'browser/extensions/extension_input_apitest.cc',
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
            'browser/pdf_browsertest.cc',
          ],
        }],
        ['OS!="linux" or toolkit_views==1', {
          'sources!': [
            'browser/extensions/browser_action_test_util_gtk.cc',
            'browser/ui/gtk/view_id_util_browsertest.cc',
          ],
        }],
        ['OS=="win"', {
          'sources': [
            '<(SHARED_INTERMEDIATE_DIR)/chrome/browser_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/common_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/renderer_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/theme_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/theme_resources_standard.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome_version/other_version.rc',
            '<(SHARED_INTERMEDIATE_DIR)/net/net_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/ui/ui_resources/ui_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/ui/ui_resources_standard/ui_resources_standard.rc',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/webkit_chromium_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/webkit_resources.rc',
            # TODO(alekseys): port sidebar to linux/mac.
            'browser/sidebar/sidebar_browsertest.cc',
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
            'browser/rlz/rlz_extension_apitest.cc',
            # http://crbug.com/15101 These tests fail on Linux and Mac.
            'browser/renderer_host/web_cache_manager_browsertest.cc',
            '../content/browser/child_process_security_policy_browsertest.cc',
          ],
        }],
        ['toolkit_uses_gtk == 1', {
          'dependencies': [
            '../build/linux/system.gyp:gtk',
            '../tools/xdisplaycheck/xdisplaycheck.gyp:xdisplaycheck',
          ],
          'sources': [
            # TODO(estade): port to win/mac.
            'browser/ui/webui/constrained_html_ui_browsertest.cc',
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
          # See the comment in this section of the unit_tests target for an
          # explanation (crbug.com/43791 - libwebcore.a is too large to mmap).
          'dependencies+++': [
            '../third_party/WebKit/Source/WebCore/WebCore.gyp/WebCore.gyp:webcore',
          ],
          'sources': [
            'browser/spellchecker/spellcheck_message_filter_browsertest.cc',
            '../content/renderer/external_popup_menu_unittest.cc',
          ],
          'sources!': [
            # TODO(hbono): This test depends on hunspell and we cannot run it on
            # Mac, which does not use hunspell by default.
            'browser/spellchecker/spellcheck_host_browsertest.cc',
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
            '../views/views.gyp:views',
          ],
          'sources!': [
            # TODO(estade): port to linux/views.
            'browser/ui/webui/constrained_html_ui_browsertest.cc',
          ],
        }, { # else: toolkit_views == 0
          'sources!': [
            'browser/extensions/browser_action_test_util_views.cc',
            'browser/tab_first_render_watcher_browsertest.cc',
            'browser/ui/panels/panel_browser_view_browsertest.cc',
            'browser/ui/views/browser_actions_container_browsertest.cc',
            'browser/ui/views/dom_view_browsertest.cc',
            'browser/ui/views/html_dialog_view_browsertest.cc',
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
      ],  # conditions
    },  # target browser_tests
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
            '<(SHARED_INTERMEDIATE_DIR)/chrome/renderer_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/theme_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/theme_resources_standard.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome_version/other_version.rc',
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
          # See crbug.com/43791 - libwebcore.a is too large to mmap on Mac.
          'dependencies+++': [
            '../third_party/WebKit/Source/WebCore/WebCore.gyp/WebCore.gyp:webcore',
          ],
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
          # See the comment in this section of the unit_tests target for an
          # explanation (crbug.com/43791 - libwebcore.a is too large to mmap).
          'dependencies+++': [
            '../third_party/WebKit/Source/WebCore/WebCore.gyp/WebCore.gyp:webcore',
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
            '../views/views.gyp:views',
          ],
        }],
      ],
    },
    {
      'target_name': 'sync_unit_tests',
      'type': 'executable',
      'sources': [
        '<(protoc_out_dir)/chrome/browser/sync/protocol/test.pb.cc',
        'app/breakpad_mac_stubs.mm',
        'browser/sync/api/sync_change_unittest.cc',
        'browser/sync/api/sync_error_unittest.cc',
        'browser/sync/engine/apply_updates_command_unittest.cc',
        'browser/sync/engine/build_commit_command_unittest.cc',
        'browser/sync/engine/clear_data_command_unittest.cc',
        'browser/sync/engine/cleanup_disabled_types_command_unittest.cc',
        'browser/sync/engine/download_updates_command_unittest.cc',
        'browser/sync/engine/model_safe_worker_unittest.cc',
        'browser/sync/engine/nigori_util_unittest.cc',
        'browser/sync/engine/process_commit_response_command_unittest.cc',
        'browser/sync/engine/syncer_proto_util_unittest.cc',
        'browser/sync/engine/sync_scheduler_unittest.cc',
        'browser/sync/engine/sync_scheduler_whitebox_unittest.cc',
        'browser/sync/engine/syncer_unittest.cc',
        'browser/sync/engine/syncproto_unittest.cc',
        'browser/sync/engine/verify_updates_command_unittest.cc',
        'browser/sync/internal_api/change_record_unittest.cc',
        'browser/sync/internal_api/debug_info_event_listener_unittest.cc',
        'browser/sync/internal_api/syncapi_mock.h',
        'browser/sync/internal_api/syncapi_server_connection_manager_unittest.cc',
        'browser/sync/internal_api/syncapi_unittest.cc',
        'browser/sync/js/js_arg_list_unittest.cc',
        'browser/sync/js/js_event_details_unittest.cc',
        'browser/sync/js/js_mutation_event_observer_unittest.cc',
        'browser/sync/js/js_sync_manager_observer_unittest.cc',
        'browser/sync/notifier/cache_invalidation_packet_handler_unittest.cc',
        'browser/sync/notifier/chrome_invalidation_client_unittest.cc',
        'browser/sync/notifier/chrome_system_resources_unittest.cc',
        'browser/sync/notifier/invalidation_notifier_unittest.cc',
        'browser/sync/notifier/non_blocking_invalidation_notifier_unittest.cc',
        'browser/sync/notifier/p2p_notifier_unittest.cc',
        'browser/sync/notifier/registration_manager_unittest.cc',
        'browser/sync/profile_sync_factory_mock.h',
        'browser/sync/protocol/proto_enum_conversions_unittest.cc',
        'browser/sync/protocol/proto_value_conversions_unittest.cc',
        'browser/sync/sessions/ordered_commit_set_unittest.cc',
        'browser/sync/sessions/session_state_unittest.cc',
        'browser/sync/sessions/status_controller_unittest.cc',
        'browser/sync/sessions/sync_session_unittest.cc',
        'browser/sync/sessions/test_util.cc',
        'browser/sync/sessions/test_util.h',
        'browser/sync/sync_js_controller_unittest.cc',
        'browser/sync/syncable/directory_backing_store_unittest.cc',
        'browser/sync/syncable/model_type_payload_map_unittest.cc',
        'browser/sync/syncable/model_type_unittest.cc',
        'browser/sync/syncable/syncable_enum_conversions_unittest.cc',
        'browser/sync/syncable/syncable_id_unittest.cc',
        'browser/sync/syncable/syncable_unittest.cc',
        'browser/sync/test/engine/fake_model_safe_worker_registrar.cc',
        'browser/sync/test/engine/fake_model_safe_worker_registrar.h',
        'browser/sync/test/engine/fake_model_worker.cc',
        'browser/sync/test/engine/fake_model_worker.h',
        'browser/sync/test/engine/mock_connection_manager.cc',
        'browser/sync/test/engine/mock_connection_manager.h',
        'browser/sync/test/engine/mock_gaia_authenticator.cc',
        'browser/sync/test/engine/mock_gaia_authenticator.h',
        'browser/sync/test/engine/mock_gaia_authenticator_unittest.cc',
        'browser/sync/test/engine/syncer_command_test.cc',
        'browser/sync/test/engine/syncer_command_test.h',
        'browser/sync/test/engine/test_id_factory.h',
        'browser/sync/test/engine/test_syncable_utils.cc',
        'browser/sync/test/engine/test_syncable_utils.h',
        'browser/sync/test/sessions/test_scoped_session_event_listener.h',
        'browser/sync/util/data_encryption_unittest.cc',
        'browser/sync/util/extensions_activity_monitor_unittest.cc',
        'browser/sync/util/protobuf_unittest.cc',
        'browser/sync/util/immutable_unittest.cc',
        'browser/sync/util/user_settings_unittest.cc',
        'browser/sync/util/weak_handle_unittest.cc',
      ],
      'include_dirs': [
        '..',
        '<(protoc_out_dir)',
      ],
      'defines' : [
        'SYNC_ENGINE_VERSION_STRING="Unknown"',
        '_CRT_SECURE_NO_WARNINGS',
        '_USE_32BIT_TIME_T',
      ],
      'dependencies': [
        'browser/sync/protocol/sync_proto.gyp:sync_proto',
        'common',
        'debugger',
        '../jingle/jingle.gyp:notifier_test_util',
        '../net/net.gyp:net',
        '../skia/skia.gyp:skia',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        '../third_party/bzip2/bzip2.gyp:bzip2',
        '../third_party/leveldatabase/leveldatabase.gyp:leveldatabase',
        '../third_party/libjingle/libjingle.gyp:libjingle',
        'syncapi_core',
        'sync_notifier',
        'test_support_common',
        'test_support_sync',
        'test_support_syncapi',
        'test_support_syncapi_service',
        'test_support_sync_notifier',
        'test_support_unit',
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
              '-lcrypt32.lib',
              '-lws2_32.lib',
              '-lsecur32.lib',
            ],
          },
          'configurations': {
            'Debug_Base': {
              'msvs_settings': {
                'VCLinkerTool': {
                  'LinkIncremental': '<(msvs_large_module_debug_link_mode)',
                },
              },
            },
          },
        }, { # else: OS != "win"
          'sources!': [
            'browser/sync/util/data_encryption_unittest.cc',
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
        ['OS=="mac"', {
          # See the comment in this section of the unit_tests target for an
          # explanation (crbug.com/43791 - libwebcore.a is too large to mmap).
          'dependencies+++': [
            '../third_party/WebKit/Source/WebCore/WebCore.gyp/WebCore.gyp:webcore',
          ],
          'dependencies': [
            'helper_app'
          ],
        },{  # OS!="mac"
          'dependencies': [
            'chrome_resources.gyp:packed_extra_resources',
            'chrome_resources.gyp:packed_resources'
          ],
        }],
        ['OS=="linux" and chromeos==1', {
          'include_dirs': [
            '<(grit_out_dir)',
          ],
        }],
      ],
    },
    {
      'target_name': 'sync_integration_tests',
      'type': 'executable',
      'dependencies': [
        'browser',
        'browser/sync/protocol/sync_proto.gyp:sync_proto',
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
        'browser/sync/test/integration/extensions_helper.cc',
        'browser/sync/test/integration/extensions_helper.h',
        'browser/sync/test/integration/many_client_bookmarks_sync_test.cc',
        'browser/sync/test/integration/many_client_passwords_sync_test.cc',
        'browser/sync/test/integration/many_client_preferences_sync_test.cc',
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
          # See the comment in this section of the unit_tests target for an
          # explanation (crbug.com/43791 - libwebcore.a is too large to mmap).
          'dependencies+++': [
            '../third_party/WebKit/Source/WebCore/WebCore.gyp/WebCore.gyp:webcore',
          ],
          # The sync_integration_tests do not run on mac without this flag.
          # Search for comments about "xcode_settings" elsewhere in this file.
          'xcode_settings': {'OTHER_LDFLAGS': ['-Wl,-ObjC']},
        }],
        ['OS=="win"', {
          'sources': [
            '<(SHARED_INTERMEDIATE_DIR)/chrome/browser_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/common_resources.rc',
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
            '../views/views.gyp:views',
          ],
        }],
      ],
    },
    {
      'target_name': 'sync_performance_tests',
      'type': 'executable',
      'dependencies': [
        'browser/sync/protocol/sync_proto.gyp:sync_proto',
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
          # See the comment in this section of the unit_tests target for an
          # explanation (crbug.com/43791 - libwebcore.a is too large to mmap).
          'dependencies+++': [
            '../third_party/WebKit/Source/WebCore/WebCore.gyp/WebCore.gyp:webcore',
          ],
          # The sync_integration_tests do not run on mac without this flag.
          # Search for comments about "xcode_settings" elsewhere in this file.
          'xcode_settings': {'OTHER_LDFLAGS': ['-Wl,-ObjC']},
        }],
        ['OS=="win"', {
          'sources': [
            '<(SHARED_INTERMEDIATE_DIR)/chrome/browser_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/common_resources.rc',
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
            '../views/views.gyp:views',
          ],
        }],
      ],
    },
    {
      # Executable that contains all the tests to be run on the GPU bots.
      'target_name': 'gpu_tests',
      'type': 'executable',
      'variables': {
        'test_list_out_dir': '<(SHARED_INTERMEDIATE_DIR)/chrome/test/gpu',
      },
      'dependencies': [
        'browser',
        'chrome',
        'chrome_resources.gyp:chrome_resources',
        'chrome_resources.gyp:chrome_strings',
        'renderer',
        'test_support_common',
        'test_support_ui',
        '../base/base.gyp:base',
        '../base/base.gyp:test_support_base',
        '../net/net.gyp:net_test_support',
        '../skia/skia.gyp:skia',
        '../testing/gtest.gyp:gtest',
        '../third_party/icu/icu.gyp:icui18n',
        '../third_party/icu/icu.gyp:icuuc',
        # Runtime dependencies
        '../third_party/mesa/mesa.gyp:osmesa',
      ],
      'include_dirs': [
        '..',
        '<(test_list_out_dir)',
      ],
      'defines': [
        'HAS_OUT_OF_PROC_TEST_RUNNER',
        'BROWSER_TESTS_HEADER_OVERRIDE="chrome/test/base/in_process_browser_test.h"',
      ],
      'sources': [
        'browser/gpu_pixel_browsertest.cc',
        'browser/gpu_crash_browsertest.cc',
        'test/base/chrome_test_launcher.cc',
        'test/gpu/webgl_conformance_tests.cc',
        'test/gpu/webgl_conformance_test_list_autogen.h',
        '../content/app/startup_helper_win.cc',
        '../content/test/test_launcher.cc',
        '../content/test/test_launcher.h',
      ],
      # hard_dependency is necessary for this target because it has actions
      # that generate a header file included by dependent targets. The header
      # file must be generated before the dependents are compiled. The usual
      # semantics are to allow the two targets to build concurrently.
      'hard_dependency': 1,
      'conditions': [
        ['OS=="win"', {
          'dependencies': [
            'chrome_version_resources',
            'installer_util_strings',
            '../sandbox/sandbox.gyp:sandbox',
          ],
          'include_dirs': [
            '<(DEPTH)/third_party/wtl/include',
          ],
          'sources': [
            'app/chrome_dll.rc',
            'app/chrome_dll_resource.h',
            'app/chrome_version.rc.version',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/browser_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/common_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/renderer_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/theme_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/theme_resources_standard.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome_version/other_version.rc',
            '<(SHARED_INTERMEDIATE_DIR)/net/net_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/ui/ui_resources/ui_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/ui/ui_resources_standard/ui_resources_standard.rc',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/webkit_chromium_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/webkit_resources.rc',
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
                  'LinkIncremental': '<(msvs_large_module_debug_link_mode)',
                },
              },
            },
          },
        }],
        ['OS=="mac"', {
          # See the comment in this section of the unit_tests target for an
          # explanation (crbug.com/43791 - libwebcore.a is too large to mmap).
          'dependencies+++': [
            '../third_party/WebKit/Source/WebCore/WebCore.gyp/WebCore.gyp:webcore',
          ],
          # See comments about "xcode_settings" elsewhere in this file.
          'xcode_settings': {'OTHER_LDFLAGS': ['-Wl,-ObjC']},
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
        ['toolkit_views==1', {
          'dependencies': [
            '../views/views.gyp:views',
          ],
        }],
      ],
    },
    {
      'target_name': 'plugin_tests',
      'type': 'executable',
      'dependencies': [
        'chrome',
        'chrome_resources.gyp:chrome_resources',
        'chrome_resources.gyp:chrome_strings',
        'test_support_common',
        'test_support_ui',
        '../skia/skia.gyp:skia',
        '../testing/gtest.gyp:gtest',
        '../third_party/libxslt/libxslt.gyp:libxslt',
        '../third_party/npapi/npapi.gyp:npapi',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'browser/plugin_test.cc',
      ],
      'conditions': [
        ['OS=="win"', {
          'dependencies': [
            'security_tests',  # run time dependency
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
        },],
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
        },
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
            'browser/net/sqlite_origin_bound_cert_store_unittest.cc',
            'browser/safe_browsing/filter_false_positive_perftest.cc',
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
              'sources!': [
                # TODO(port):
                'browser/safe_browsing/filter_false_positive_perftest.cc',
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
                '../views/views.gyp:views',
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
            'syncapi_core',
            '../base/base.gyp:base',
            '../net/net.gyp:net_test_support',
            '../skia/skia.gyp:skia',
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
    ['OS == "mac" or OS == "win" or (os_posix == 1 and target_arch == python_arch)', {
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
            'syncapi_core',
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
          'cflags': [
             '-Wno-uninitialized',
             '-Wno-self-assign',  # to keep clang happy for generated code.
          ],
          'libraries': [
            '<@(pyautolib_libraries)',
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
            # When generated, pyautolib_wrap.cc includes some swig support
            # files which, as of swig 1.3.31 that comes with 10.5 and 10.6,
            # may not compile cleanly at -Wall.
            'GCC_TREAT_WARNINGS_AS_ERRORS': 'NO',  # -Wno-error

            # Link with python2.6. Using -L/usr/lib and -lpython2.6 does not
            # work with the -isysroot argument passed in. Even if it did,
            # the linker shouldn't use any other lib not in the 10.5 sdk.
            'OTHER_LDFLAGS': [
              '/usr/lib/libpython2.6.dylib'
            ],
          },
          'conditions': [
            ['os_posix == 1 and OS!="mac"', {
              'include_dirs': [
                '..',
                '<(sysroot)/usr/include/python<(python_ver)',
              ],
              'dependencies': [
                '../build/linux/system.gyp:gtk',
              ],
              'link_settings': {
                'libraries': [
                  '-lpython<(python_ver)',
                ],
              },
            }],
            ['OS=="mac"', {
              # See the comment in this section of the unit_tests target for an
              # explanation (crbug.com/43791 - libwebcore.a is too large to
              # mmap).
              'dependencies+++': [
                '../third_party/WebKit/Source/WebCore/WebCore.gyp/WebCore.gyp:webcore',
              ],
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
            # ui_tests seem unhappy on both Mac and Win when run under
            # coverage (all tests fail, often with a
            # "server_->WaitForInitialLoads()").  TODO(jrg):
            # investigate why.
            # 'ui_tests',
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
                # Reason for disabling UI tests on non-Linux above.
                'ui_tests',
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
