# Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
    ],
  },
  'targets': [
    {
      # This target contains mocks and test utilities that don't belong in
      # production libraries but are used by more than one test executable.
      'target_name': 'test_support_common',
      'type': '<(library)',
      'dependencies': [
        'browser',
        'common',
        'renderer',
        'chrome_gpu',
        'chrome_resources',
        'chrome_strings',
        'app/policy/cloud_policy_codegen.gyp:policy',
        'browser/sync/protocol/sync_proto.gyp:sync_proto_cpp',
        'theme_resources',
        '../base/base.gyp:test_support_base',
        '../ipc/ipc.gyp:test_support_ipc',
        '../media/media.gyp:media_test_support',
        # 'test/test_url_request_context_getter.h' brings in this requirement.
        '../net/net.gyp:net_test_support',
        '../skia/skia.gyp:skia',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
      ],
      'export_dependent_settings': [
        'renderer',
        'app/policy/cloud_policy_codegen.gyp:policy',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'app/breakpad_mac_stubs.mm',
        'browser/autofill/autofill_common_test.cc',
        'browser/autofill/autofill_common_test.h',
        'browser/autofill/data_driven_test.cc',
        'browser/autofill/data_driven_test.h',
        # The only thing used from browser is Browser::Type.
        'browser/extensions/test_extension_prefs.cc',
        'browser/extensions/test_extension_prefs.h',
        'browser/geolocation/arbitrator_dependency_factories_for_test.cc',
        'browser/geolocation/arbitrator_dependency_factories_for_test.h',
        'browser/geolocation/mock_location_provider.cc',
        'browser/geolocation/mock_location_provider.h',
        'browser/mock_browsing_data_appcache_helper.cc',
        'browser/mock_browsing_data_appcache_helper.h',
        'browser/mock_browsing_data_database_helper.cc',
        'browser/mock_browsing_data_database_helper.h',
        'browser/mock_browsing_data_indexed_db_helper.cc',
        'browser/mock_browsing_data_indexed_db_helper.h',
        'browser/mock_browsing_data_local_storage_helper.cc',
        'browser/mock_browsing_data_local_storage_helper.h',
        # TODO:  these should live here but are currently used by
        # production code code in libbrowser (in chrome.gyp).
        #'browser/net/url_request_mock_http_job.cc',
        #'browser/net/url_request_mock_http_job.h',
        'browser/net/url_request_mock_net_error_job.cc',
        'browser/net/url_request_mock_net_error_job.h',
        'browser/notifications/notification_test_util.cc',
        'browser/notifications/notification_test_util.h',
        'browser/prefs/pref_observer_mock.cc',
        'browser/prefs/pref_observer_mock.h',
        'browser/prefs/pref_service_mock_builder.cc',
        'browser/prefs/pref_service_mock_builder.h',
        'browser/prefs/testing_pref_store.cc',
        'browser/prefs/testing_pref_store.h',
        'browser/sessions/session_service_test_helper.cc',
        'browser/sessions/session_service_test_helper.h',
        'browser/sync/profile_sync_service_mock.cc',
        'browser/sync/profile_sync_service_mock.h',
        'browser/sync/syncable/syncable_mock.cc',
        'browser/sync/syncable/syncable_mock.h',
        'browser/ui/browser.h',
        'browser/ui/cocoa/browser_test_helper.cc',
        'browser/ui/cocoa/browser_test_helper.h',
        'browser/ui/tab_contents/test_tab_contents_wrapper.cc',
        'browser/ui/tab_contents/test_tab_contents_wrapper.h',
        'common/net/test_url_fetcher_factory.cc',
        'common/net/test_url_fetcher_factory.h',
        'common/notification_observer_mock.cc',
        'common/notification_observer_mock.h',
        'common/pref_store_observer_mock.cc',
        'common/pref_store_observer_mock.h',
        'renderer/mock_keyboard.cc',
        'renderer/mock_keyboard.h',
        'renderer/mock_keyboard_driver_win.cc',
        'renderer/mock_keyboard_driver_win.h',
        'renderer/mock_printer.cc',
        'renderer/mock_printer.h',
        'renderer/mock_render_process.cc',
        'renderer/mock_render_process.h',
        'renderer/mock_render_thread.cc',
        'renderer/mock_render_thread.h',
        'renderer/safe_browsing/mock_feature_extractor_clock.cc',
        'renderer/safe_browsing/mock_feature_extractor_clock.h',
        'test/automation/autocomplete_edit_proxy.cc',
        'test/automation/autocomplete_edit_proxy.h',
        'test/automation/automation_handle_tracker.cc',
        'test/automation/automation_handle_tracker.h',
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
        'test/automation/window_proxy.cc',
        'test/automation/window_proxy.h',
        'test/bookmark_load_observer.cc',
        'test/bookmark_load_observer.h',
        'test/chrome_process_util.cc',
        'test/chrome_process_util.h',
        'test/chrome_process_util_mac.cc',
        'test/in_process_browser_test.cc',
        'test/in_process_browser_test.h',
        'test/model_test_utils.cc',
        'test/model_test_utils.h',
        'test/profile_mock.cc',
        'test/profile_mock.h',
        'test/signaling_task.cc',
        'test/signaling_task.h',
        'test/test_browser_window.cc',
        'test/test_browser_window.h',
        'test/test_launcher_utils.cc',
        'test/test_launcher_utils.h',
        'test/test_location_bar.cc',
        'test/test_location_bar.h',
        'test/test_switches.cc',
        'test/test_switches.h',
        'test/test_url_request_context_getter.cc',
        'test/test_url_request_context_getter.h',
        'test/testing_browser_process.cc',
        'test/testing_browser_process.h',
        'test/testing_browser_process_test.h',
        'test/testing_pref_service.cc',
        'test/testing_pref_service.h',
        'test/testing_profile.cc',
        'test/testing_profile.h',
        'test/thread_observer_helper.h',
        'test/thread_test_helper.cc',
        'test/thread_test_helper.h',
        'test/ui_test_utils.cc',
        'test/ui_test_utils.h',
        'test/ui_test_utils_linux.cc',
        'test/ui_test_utils_mac.mm',
        'test/ui_test_utils_win.cc',
        'test/unit/chrome_test_suite.cc',
        'test/unit/chrome_test_suite.h',
        'test/values_test_util.cc',
        'test/values_test_util.h',
        '../content/browser/renderer_host/mock_render_process_host.cc',
        '../content/browser/renderer_host/mock_render_process_host.h',
        '../content/browser/renderer_host/test_backing_store.cc',
        '../content/browser/renderer_host/test_backing_store.h',
        '../content/browser/renderer_host/test_render_view_host.cc',
        '../content/browser/renderer_host/test_render_view_host.h',
        '../content/browser/tab_contents/test_tab_contents.cc',
        '../content/browser/tab_contents/test_tab_contents.h',
      ],
      'conditions': [
        ['OS=="linux"', {
          'dependencies': [
            '../build/linux/system.gyp:gtk',
            '../build/linux/system.gyp:nss',
          ],
        }],
        ['OS=="win"', {
          'include_dirs': [
            '<(DEPTH)/third_party/wtl/include',
          ],
        }],
      ],
    },
    {
      'target_name': 'test_support_ui',
      'type': '<(library)',
      'dependencies': [
        'test_support_common',
        'chrome_resources',
        'chrome_strings',
        'theme_resources',
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
        'test/automated_ui_tests/automated_ui_test_base.cc',
        'test/automated_ui_tests/automated_ui_test_base.h',
        'test/automation/proxy_launcher.cc',
        'test/automation/proxy_launcher.h',
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
          ],
        }],
        ['OS=="linux"', {
          'dependencies': [
            '../build/linux/system.gyp:gtk',
          ],
        }],
      ],
    },
    {
      'target_name': 'test_support_sync',
      'type': '<(library)',
      'dependencies': [
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        'sync',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'browser/sync/js_test_util.cc',
        'browser/sync/js_test_util.h',
      ],
    },
    {
      'target_name': 'test_support_unit',
      'type': '<(library)',
      'dependencies': [
        'test_support_common',
        'chrome_resources',
        'chrome_strings',
        '../skia/skia.gyp:skia',
        '../testing/gtest.gyp:gtest',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'test/unit/run_all_unittests.cc',
      ],
      'conditions': [
        ['OS=="linux"', {
          'dependencies': [
            # Needed for the following #include chain:
            #   test/unit/run_all_unittests.cc
            #   test/unit/chrome_test_suite.h
            #   gtk/gtk.h
            '../build/linux/system.gyp:gtk',
          ],
        }],
      ],
    },
    {
      'target_name': 'automated_ui_tests',
      'type': 'executable',
      'msvs_guid': 'D2250C20-3A94-4FB9-AF73-11BC5B73884B',
      'dependencies': [
        'browser',
        'renderer',
        'test_support_common',
        'test_support_ui',
        'theme_resources',
        '../base/base.gyp:base',
        '../skia/skia.gyp:skia',
        '../third_party/libxml/libxml.gyp:libxml',
        '../testing/gtest.gyp:gtest',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'test/automated_ui_tests/automated_ui_test_interactive_test.cc',
        'test/automated_ui_tests/automated_ui_tests.cc',
        'test/automated_ui_tests/automated_ui_tests.h',
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
        ['OS=="linux"', {
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
      'msvs_guid': '018D4F38-6272-448F-A864-976DA09F05D0',
      'dependencies': [
        'browser/sync/protocol/sync_proto.gyp:sync_proto_cpp',
        'chrome',
        'chrome_resources',
        'chrome_strings',
        'debugger',
        'syncapi',
        'test_support_common',
        'test_support_ui',
        '../third_party/hunspell/hunspell.gyp:hunspell',
        '../net/net.gyp:net_resources',
        '../net/net.gyp:net_test_support',
        '../skia/skia.gyp:skia',
        '../third_party/icu/icu.gyp:icui18n',
        '../third_party/libpng/libpng.gyp:libpng',
        '../third_party/zlib/zlib.gyp:zlib',
        '../testing/gtest.gyp:gtest',
        '../third_party/npapi/npapi.gyp:npapi',
        # run time dependency
        '../webkit/support/webkit_support.gyp:webkit_resources',
      ],
      'include_dirs': [
        '..',
      ],
      'defines': [ 'ALLOW_IN_PROC_BROWSER_TEST' ],
      'sources': [
        'browser/accessibility/accessibility_mac_uitest.mm',
        'browser/autocomplete/autocomplete_edit_view_browsertest.cc',
        'browser/autofill/autofill_browsertest.cc',
        'browser/browser_focus_uitest.cc',
        'browser/browser_keyevents_browsertest.cc',
        'browser/collected_cookies_uitest.cc',
        'browser/debugger/devtools_sanity_unittest.cc',
        'browser/ui/gtk/bookmark_bar_gtk_interactive_uitest.cc',
        'browser/instant/instant_browsertest.cc',
        'browser/notifications/notifications_interactive_uitest.cc',
        'browser/ui/views/bookmark_bar_view_test.cc',
        'browser/ui/views/find_bar_host_interactive_uitest.cc',
        'browser/ui/views/tabs/tab_dragging_test.cc',
        'test/interactive_ui/fast_shutdown_interactive_uitest.cc',
        'test/interactive_ui/infobars_uitest.cc',
        'test/interactive_ui/keyboard_access_uitest.cc',
        'test/interactive_ui/mouseleave_interactive_uitest.cc',
        'test/interactive_ui/npapi_interactive_test.cc',
        'test/interactive_ui/view_event_test_base.cc',
        'test/interactive_ui/view_event_test_base.h',
        'test/out_of_proc_test_runner.cc',
        'test/unit/chrome_test_suite.h',
      ],
      'conditions': [
        ['OS=="linux"', {
          'dependencies': [
            '../build/linux/system.gyp:gtk',
            '../build/linux/system.gyp:nss',
            '../tools/xdisplaycheck/xdisplaycheck.gyp:xdisplaycheck',
          ],
        }],
        ['OS=="linux" and toolkit_views==0', {
          'sources!': [
            # TODO(port)
            'browser/ui/views/bookmark_bar_view_test.cc',
            'browser/ui/views/find_bar_host_interactive_uitest.cc',
            'browser/ui/views/tabs/tab_dragging_test.cc',
            'browser/ui/views/tabs/tab_strip_interactive_uitest.cc',
            'test/interactive_ui/npapi_interactive_test.cc',
            'test/interactive_ui/view_event_test_base.cc',
            'test/interactive_ui/view_event_test_base.h',
          ],
        }],
        ['OS=="linux" and toolkit_views==1', {
          'sources!': [
            'browser/ui/gtk/bookmark_bar_gtk_interactive_uitest.cc',
            # TODO(port)
            'test/interactive_ui/npapi_interactive_test.cc',
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
            'browser/autocomplete/autocomplete_edit_view_browsertest.cc',
            'browser/debugger/devtools_sanity_unittest.cc',
            'browser/ui/views/bookmark_bar_view_test.cc',
            'browser/ui/views/find_bar_host_interactive_uitest.cc',
            'browser/ui/views/tabs/tab_dragging_test.cc',
            'browser/ui/views/tabs/tab_strip_interactive_uitest.cc',
            'test/interactive_ui/npapi_interactive_test.cc',
            'test/interactive_ui/view_event_test_base.cc',
            'test/interactive_ui/view_event_test_base.h',
          ],
          # See comment about the same line in chrome/chrome_tests.gypi.
          'xcode_settings': {'OTHER_LDFLAGS': ['-Wl,-ObjC']},
        }],  # OS=="mac"
        ['toolkit_views==1', {
          'dependencies': [
            '../views/views.gyp:views',
          ],
        }],
        ['OS=="win"', {
          'include_dirs': [
            '../third_party/wtl/include',
          ],
          'dependencies': [
            '../app/app.gyp:app_resources',
            'chrome.gyp:chrome_dll_version',
            'chrome.gyp:installer_util_strings',
            '../sandbox/sandbox.gyp:sandbox',
            '../third_party/iaccessible2/iaccessible2.gyp:iaccessible2',
            '../third_party/isimpledom/isimpledom.gyp:isimpledom',
          ],
          'sources': [
            '../webkit/glue/resources/aliasb.cur',
            '../webkit/glue/resources/cell.cur',
            '../webkit/glue/resources/col_resize.cur',
            '../webkit/glue/resources/copy.cur',
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
            '<(SHARED_INTERMEDIATE_DIR)/app/app_resources/app_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/autofill_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/browser_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/common_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/renderer_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/theme_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome_dll_version/chrome_dll_version.rc',
            '<(SHARED_INTERMEDIATE_DIR)/net/net_resources.rc',
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
        }],  # OS=="win"
      ],  # conditions
    },
    {
      'target_name': 'ui_tests',
      'type': 'executable',
      'msvs_guid': '76235B67-1C27-4627-8A33-4B2E1EF93EDE',
      'dependencies': [
        'chrome',
        'browser',
        'common',
        'chrome_resources',
        'chrome_strings',
        'profile_import',
        'test_support_ui',
        '../app/app.gyp:app_base',
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
        'chrome_mesa',
        'default_plugin/default_plugin.gyp:default_plugin',
        '../ppapi/ppapi.gyp:ppapi_tests',
        '../third_party/WebKit/Source/WebKit/chromium/WebKit.gyp:copy_TestNetscapePlugIn',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'app/chrome_main_uitest.cc',
        'browser/browser_encoding_uitest.cc',
        'browser/download/download_uitest.cc',
        'browser/download/save_page_uitest.cc',
        'browser/errorpage_uitest.cc',
        'browser/default_plugin_uitest.cc',
        'browser/extensions/extension_uitest.cc',
        'browser/history/multipart_uitest.cc',
        'browser/history/redirect_uitest.cc',
        'browser/iframe_uitest.cc',
        'browser/images_uitest.cc',
        'browser/locale_tests_uitest.cc',
        'browser/media_uitest.cc',
        'browser/metrics/metrics_service_uitest.cc',
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
        'browser/webui/bookmarks_ui_uitest.cc',
        'browser/webui/new_tab_ui_uitest.cc',
        'browser/webui/options/options_ui_uitest.cc',
        'browser/webui/print_preview_ui_uitest.cc',
        'common/logging_chrome_uitest.cc',
        'renderer/external_extension_uitest.cc',
        'test/automation/automation_proxy_uitest.cc',
        'test/automation/automation_proxy_uitest.h',
        'test/automation/extension_proxy_uitest.cc',
        'test/automated_ui_tests/automated_ui_test_test.cc',
        'test/chrome_process_util_uitest.cc',
        'test/gpu/gpu_uitest.cc',
        'test/ui/dom_checker_uitest.cc',
        'test/ui/dromaeo_benchmark_uitest.cc',
        'test/ui/history_uitest.cc',
        'test/ui/layout_plugin_uitest.cc',
        'test/ui/named_interface_uitest.cc',
        'test/ui/npapi_uitest.cc',
        'test/ui/omnibox_uitest.cc',
        'test/ui/pepper_uitest.cc',
        'test/ui/ppapi_uitest.cc',
        'test/ui/sandbox_uitests.cc',
        'test/ui/sunspider_uitest.cc',
        'test/ui/v8_benchmark_uitest.cc',
        'worker/worker_uitest.cc',
        '../content/browser/appcache/appcache_ui_test.cc',
        '../content/browser/in_process_webkit/dom_storage_uitest.cc',
        '../content/browser/renderer_host/resource_dispatcher_host_uitest.cc',
      ],
      'conditions': [
        ['target_arch!="arm"', {
          'dependencies': [
            '../webkit/webkit.gyp:copy_npapi_test_plugin',
          ],
        }],
        ['OS=="linux"', {
          'dependencies': [
            '../build/linux/system.gyp:gtk',
            '../tools/xdisplaycheck/xdisplaycheck.gyp:xdisplaycheck',
          ],
        }, { # else: OS != "linux"
          'sources!': [
            'browser/process_singleton_linux_uitest.cc',
          ],
        }],
        ['OS=="linux" and toolkit_views==1', {
          'sources!': [
            'browser/download/download_uitest.cc',
          ],
        }],
        ['toolkit_views==1', {
          'dependencies': [
            '../views/views.gyp:views',
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
          'sources!': [
            # TODO(dtu): port to windows http://crosbug.com/8515
            'test/ui/named_interface_uitest.cc',
          ],
        }, { # else: OS != "win"
          'sources!': [
            # TODO(port): http://crbug.com/45770
            'browser/printing/printing_layout_uitest.cc',
          ],
        }],
        ['OS=="linux" or OS=="freebsd"', {
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
             'browser/webui/print_preview_ui_uitest.cc',
          ],
        }],
      ],
    },
    {
      # chromedriver is the chromium impelmentation of the WebDriver
      # wire protcol.  A description of the WebDriver and examples can
      # be found at: http://seleniumhq.org/docs/09_webdriver.html.
      # The documention of the protocol implemented is at:
      # http://code.google.com/p/selenium/wiki/JsonWireProtocol
      'target_name': 'chromedriver_lib',
      'type': '<(library)',
      'dependencies': [
        'browser',
        'chrome',
        'chrome_resources',
        'chrome_strings',
        'common',
        'syncapi',
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
        '../third_party/mongoose/mongoose.h',
        '../third_party/mongoose/mongoose.c',
        '../third_party/webdriver/atoms.h',
        'test/webdriver/automation.h',
        'test/webdriver/automation.cc',
        'test/webdriver/cookie.h',
        'test/webdriver/cookie.cc',
        'test/webdriver/dispatch.h',
        'test/webdriver/dispatch.cc',
        'test/webdriver/error_codes.h',
        'test/webdriver/http_response.h',
        'test/webdriver/http_response.cc',
        'test/webdriver/keycode_text_conversion.h',
        'test/webdriver/keycode_text_conversion_linux.cc',
        'test/webdriver/keycode_text_conversion_mac.mm',
        'test/webdriver/keycode_text_conversion_win.cc',
        'test/webdriver/keymap.h',
        'test/webdriver/keymap.cc',
        'test/webdriver/session.h',
        'test/webdriver/session.cc',
        'test/webdriver/session_manager.h',
        'test/webdriver/session_manager.cc',
        'test/webdriver/utility_functions.h',
        'test/webdriver/utility_functions.cc',
        'test/webdriver/webdriver_key_converter.h',
        'test/webdriver/webdriver_key_converter.cc',
        'test/webdriver/web_element_id.h',
        'test/webdriver/web_element_id.cc',
        'test/webdriver/commands/command.h',
        'test/webdriver/commands/command.cc',
        'test/webdriver/commands/cookie_commands.h',
        'test/webdriver/commands/cookie_commands.cc',
        'test/webdriver/commands/create_session.h',
        'test/webdriver/commands/create_session.cc',
        'test/webdriver/commands/execute_command.h',
        'test/webdriver/commands/execute_command.cc',
        'test/webdriver/commands/find_element_commands.h',
        'test/webdriver/commands/find_element_commands.cc',
        'test/webdriver/commands/implicit_wait_command.h',
        'test/webdriver/commands/implicit_wait_command.cc',
        'test/webdriver/commands/navigate_commands.h',
        'test/webdriver/commands/navigate_commands.cc',
        'test/webdriver/commands/mouse_commands.h',
        'test/webdriver/commands/mouse_commands.cc',
        'test/webdriver/commands/response.cc',
        'test/webdriver/commands/response.h',
        'test/webdriver/commands/session_with_id.h',
        'test/webdriver/commands/session_with_id.cc',
        'test/webdriver/commands/source_command.h',
        'test/webdriver/commands/source_command.cc',
        'test/webdriver/commands/speed_command.h',
        'test/webdriver/commands/speed_command.cc',
        'test/webdriver/commands/target_locator_commands.h',
        'test/webdriver/commands/target_locator_commands.cc',
        'test/webdriver/commands/title_command.h',
        'test/webdriver/commands/title_command.cc',
        'test/webdriver/commands/url_command.h',
        'test/webdriver/commands/url_command.cc',
        'test/webdriver/commands/webdriver_command.h',
        'test/webdriver/commands/webdriver_command.cc',
        'test/webdriver/commands/webelement_commands.h',
        'test/webdriver/commands/webelement_commands.cc',
      ],
      'conditions': [
        ['OS=="linux"', {
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
        ['OS=="linux" or OS=="freebsd"', {
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
      'msvs_guid': '3F9C9B6D-BBB6-480F-B038-23BF35A432DC',
      'dependencies': [
        'chromedriver_lib',
        '../skia/skia.gyp:skia',
        '../testing/gtest.gyp:gtest',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'test/webdriver/server.cc',
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
      'msvs_guid': 'E24B445D-96E3-4272-BB54-AACBC6D3FE7E',
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
        'test/webdriver/commands/implicit_wait_command_unittest.cc',
        'test/webdriver/dispatch_unittest.cc',
        'test/webdriver/http_response_unittest.cc',
        'test/webdriver/keycode_text_conversion_unittest.cc',
        'test/webdriver/utility_functions_unittest.cc',
        'test/webdriver/webdriver_key_converter_unittest.cc',
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
      'target_name': 'nacl_security_tests',
      'type': 'shared_library',
      'msvs_guid': 'D705E8B8-4750-4F1F-BC8F-A7806872F504',
      'include_dirs': [
        '..'
      ],
      'sources': [
       # mostly OS dependent files below...
      ],
      'conditions': [
        ['OS=="mac"', {
          # Only the Mac version uses gtest (linking issues on other platforms).
          'dependencies': [
            '../testing/gtest.gyp:gtest'
          ],
          'sources': [
            'test/nacl_security_tests/commands_posix.cc',
            'test/nacl_security_tests/commands_posix.h',
            'test/nacl_security_tests/nacl_security_tests_posix.h',
            'test/nacl_security_tests/nacl_security_tests_mac.cc',
          ],
          'xcode_settings': {
             'DYLIB_INSTALL_NAME_BASE': '@executable_path/',
          },
        },],
        ['OS=="linux"', {
          'sources': [
            'test/nacl_security_tests/commands_posix.cc',
            'test/nacl_security_tests/commands_posix.h',
            'test/nacl_security_tests/nacl_security_tests_posix.h',
            'test/nacl_security_tests/nacl_security_tests_linux.cc',
          ],
        },],
        ['OS=="win"', {
          'sources': [
            '../sandbox/tests/validation_tests/commands.cc',
            '../sandbox/tests/validation_tests/commands.h',
            '../sandbox/tests/common/controller.h',
            'test/nacl_security_tests/nacl_security_tests_win.h',
            'test/nacl_security_tests/nacl_security_tests_win.cc',
          ],
        },],
        # Set fPIC in case it isn't set.
        ['(OS=="linux" or OS=="openbsd" or OS=="freebsd" or OS=="solaris")'
         'and (target_arch=="x64" or target_arch=="arm") and linux_fpic!=1', {
          'cflags': ['-fPIC'],
        },],
      ],
    },
    {
      'target_name': 'nacl_sandbox_tests',
      'type': 'executable',
      'msvs_guid': '3087FC25-2C24-44B2-8253-44065EB47ACD',
      'dependencies': [
        'chrome',
        'browser',
        'common',
        'chrome_resources',
        'chrome_strings',
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
        'test/nacl/nacl_test.cc',
        'test/nacl/nacl_sandbox_test.cc'
      ],
      'conditions': [
        ['OS=="win"', {
          'dependencies': [
            'chrome_nacl_win64',
            'nacl_security_tests', # run time dependency
            'nacl_security_tests64', # run time dependency
            'test_support_common',
            '../google_update/google_update.gyp:google_update',
            '../views/views.gyp:views',
            # run time dependency
            '../webkit/webkit.gyp:copy_npapi_test_plugin',
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
                  'LinkIncremental': '<(msvs_debug_link_nonincremental)',
                },
              },
            },
          },
        }],
        ['OS=="mac"', {
          'dependencies': [
            'nacl_security_tests', # run time dependency
          ],
        }],
      ],
    },
    {
      'target_name': 'nacl_ui_tests',
      'type': 'executable',
      'msvs_guid': '43E2004F-CD62-4595-A8A6-31E9BFA1EE5E',
      'dependencies': [
        'chrome',
        'browser',
        'common',
        'chrome_resources',
        'chrome_strings',
        'test_support_ui',
        '../base/base.gyp:base',
        '../build/temp_gyp/googleurl.gyp:googleurl',
        '../net/net.gyp:net',
        '../skia/skia.gyp:skia',
        '../testing/gtest.gyp:gtest',
        '../third_party/icu/icu.gyp:icui18n',
        '../third_party/icu/icu.gyp:icuuc',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'test/nacl/nacl_test.cc',
        'test/nacl/nacl_ui_test.cc',
      ],
      'conditions': [
        ['OS=="win"', {
          'dependencies': [
            'chrome_nacl_win64',
            'security_tests',  # run time dependency
            'test_support_common',
            '../google_update/google_update.gyp:google_update',
            # run time dependency
            '../webkit/webkit.gyp:copy_npapi_test_plugin',
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
                  'LinkIncremental': '<(msvs_debug_link_nonincremental)',
                },
              },
            },
          },
        }],
        ['toolkit_views==1', {
          'dependencies': [
            '../views/views.gyp:views',
          ],
        }],
      ],
    },
    {
      'target_name': 'unit_tests',
      'type': 'executable',
      'msvs_guid': 'ECFC2BEC-9FC0-4AD9-9649-5F26793F65FC',
      'dependencies': [
        'browser',
        'browser/sync/protocol/sync_proto.gyp:sync_proto_cpp',
        'chrome',
        'chrome_gpu',
        'chrome_resources',
        'chrome_strings',
        'common',
        'profile_import',
        'renderer',
        'service',
        'test_support_common',
        'test_support_sync',
        'test_support_unit',
        'utility',
        '../app/app.gyp:app_base',
        '../app/app.gyp:app_resources',
        '../gpu/gpu.gyp:gpu_unittest_utils',
        '../ipc/ipc.gyp:ipc',
        '../media/media.gyp:media_test_support',
        '../net/net.gyp:net_resources',
        '../net/net.gyp:net_test_support',
        '../printing/printing.gyp:printing',
        '../webkit/support/webkit_support.gyp:webkit_resources',
        '../skia/skia.gyp:skia',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        '../third_party/bzip2/bzip2.gyp:bzip2',
        '../third_party/cld/cld.gyp:cld',
        '../third_party/expat/expat.gyp:expat',
        '../third_party/icu/icu.gyp:icui18n',
        '../third_party/icu/icu.gyp:icuuc',
        '../third_party/libjingle/libjingle.gyp:libjingle',
        '../third_party/libxml/libxml.gyp:libxml',
        '../third_party/npapi/npapi.gyp:npapi',
        '../third_party/WebKit/Source/WebKit/chromium/WebKit.gyp:webkit',
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
        '<(protoc_out_dir)/chrome/browser/history/in_memory_url_index_cache.pb.cc',
        'app/breakpad_mac_stubs.mm',
        'app/chrome_dll.rc',
        # All unittests in browser, common, renderer and service.
        'browser/about_flags_unittest.cc',
        'browser/accessibility/browser_accessibility_mac_unittest.mm',
        'browser/accessibility/browser_accessibility_win_unittest.cc',
        'browser/app_controller_mac_unittest.mm',
        'browser/autocomplete_history_manager_unittest.cc',
        'browser/autocomplete/autocomplete_edit_unittest.cc',
        'browser/autocomplete/autocomplete_edit_view_mac_unittest.mm',
        'browser/autocomplete/autocomplete_popup_view_gtk_unittest.cc',
        'browser/autocomplete/autocomplete_popup_view_mac_unittest.mm',
        'browser/autocomplete/autocomplete_result_unittest.cc',
        'browser/autocomplete/autocomplete_unittest.cc',
        'browser/autocomplete/history_contents_provider_unittest.cc',
        'browser/autocomplete/history_quick_provider_unittest.cc',
        'browser/autocomplete/history_url_provider_unittest.cc',
        'browser/autocomplete/keyword_provider_unittest.cc',
        'browser/autocomplete/search_provider_unittest.cc',
        'browser/autofill/address_unittest.cc',
        'browser/autofill/address_field_unittest.cc',
        'browser/autofill/autofill_address_model_mac_unittest.mm',
        'browser/autofill/autofill_address_sheet_controller_mac_unittest.mm',
        'browser/autofill/autofill_credit_card_model_mac_unittest.mm',
        'browser/autofill/autofill_credit_card_sheet_controller_mac_unittest.mm',
        'browser/autofill/autofill_country_unittest.cc',
        'browser/autofill/autofill_dialog_controller_mac_unittest.mm',
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
        'browser/autofill/select_control_handler_unittest.cc',
        'browser/automation/automation_provider_unittest.cc',
        'browser/background_application_list_model_unittest.cc',
        'browser/background_contents_service_unittest.cc',
        'browser/background_mode_manager_unittest.cc',
        'browser/background_page_tracker_unittest.cc',
        'browser/bookmarks/bookmark_codec_unittest.cc',
        'browser/bookmarks/bookmark_context_menu_controller_unittest.cc',
        'browser/bookmarks/bookmark_html_writer_unittest.cc',
        'browser/bookmarks/bookmark_index_unittest.cc',
        'browser/bookmarks/bookmark_model_test_utils.cc',
        'browser/bookmarks/bookmark_model_test_utils.h',
        'browser/bookmarks/bookmark_model_unittest.cc',
        'browser/bookmarks/bookmark_node_data_unittest.cc',
        'browser/bookmarks/bookmark_utils_unittest.cc',
        'browser/browser_about_handler_unittest.cc',
        'browser/browser_commands_unittest.cc',
        'browser/browsing_data_appcache_helper_unittest.cc',
        'browser/browsing_data_database_helper_unittest.cc',
        'browser/browsing_data_indexed_db_helper_unittest.cc',
        'browser/browsing_data_local_storage_helper_unittest.cc',
        'browser/chrome_browser_application_mac_unittest.mm',
        'browser/chrome_plugin_unittest.cc',
        'browser/chromeos/customization_document_unittest.cc',
        'browser/chromeos/external_metrics_unittest.cc',
        'browser/chromeos/gview_request_interceptor_unittest.cc',
        'browser/chromeos/input_method/input_method_util_unittest.cc',
        'browser/chromeos/language_preferences_unittest.cc',
        'browser/chromeos/login/authenticator_unittest.cc',
        'browser/chromeos/login/cookie_fetcher_unittest.cc',
        'browser/chromeos/login/cryptohome_op_unittest.cc',
        'browser/chromeos/login/google_authenticator_unittest.cc',
        'browser/chromeos/login/mock_auth_attempt_state_resolver.h',
        'browser/chromeos/login/mock_auth_response_handler.cc',
        'browser/chromeos/login/mock_login_status_consumer.h',
        'browser/chromeos/login/mock_url_fetchers.h',
        'browser/chromeos/login/mock_url_fetchers.cc',
        'browser/chromeos/login/mock_user_manager.h',
        'browser/chromeos/login/online_attempt_unittest.cc',
        'browser/chromeos/login/owner_key_utils_unittest.cc',
        'browser/chromeos/login/owner_manager_unittest.cc',
        'browser/chromeos/login/ownership_service_unittest.cc',
        'browser/chromeos/login/parallel_authenticator_unittest.cc',
        'browser/chromeos/login/signed_settings_unittest.cc',
        'browser/chromeos/login/signed_settings_helper_unittest.cc',
        'browser/chromeos/login/signed_settings_temp_storage_unittest.cc',
        'browser/chromeos/login/user_controller_unittest.cc',
        'browser/chromeos/login/wizard_accessibility_handler_unittest.cc',
        'browser/chromeos/network_message_observer_unittest.cc',
        'browser/chromeos/notifications/desktop_notifications_unittest.cc',
        'browser/chromeos/offline/offline_load_page_unittest.cc',
        'browser/chromeos/plugin_selection_policy_unittest.cc',
        'browser/chromeos/proxy_config_service_impl_unittest.cc',
        'browser/chromeos/status/input_method_menu_unittest.cc',
        'browser/chromeos/version_loader_unittest.cc',
        'browser/chromeos/webui/login/authenticator_facade_cros_unittest.cc',
        'browser/chromeos/webui/login/login_ui_unittest.cc',
        'browser/chromeos/webui/login/mock_authenticator_facade_cros.h',
        'browser/chromeos/webui/login/mock_authenticator_facade_cros_helpers.h',
        'browser/chromeos/webui/login/mock_login_ui_helpers.h',
        'browser/content_setting_bubble_model_unittest.cc',
        'browser/content_setting_image_model_unittest.cc',
        'browser/debugger/devtools_remote_listen_socket_unittest.cc',
        'browser/debugger/devtools_remote_listen_socket_unittest.h',
        'browser/debugger/devtools_remote_message_unittest.cc',
        'browser/diagnostics/diagnostics_model_unittest.cc',
        'browser/cocoa/keystone_glue_unittest.mm',
        'browser/command_updater_unittest.cc',
        'browser/content_exceptions_table_model_unittest.cc',
        'browser/content_settings/content_settings_pattern_unittest.cc',
        'browser/content_settings/content_settings_provider_unittest.cc',
        'browser/content_settings/host_content_settings_map_unittest.cc',
        'browser/content_settings/content_settings_mock_provider.cc',
        'browser/content_settings/content_settings_mock_provider.h',
        'browser/content_settings/content_settings_policy_provider_unittest.cc',
        'browser/content_settings/content_settings_pref_provider_unittest.cc',
        'browser/content_settings/stub_settings_observer.cc',
        'browser/content_settings/stub_settings_observer.h',
        'browser/cookies_tree_model_unittest.cc',
        'browser/debugger/devtools_manager_unittest.cc',
        'browser/download/base_file_unittest.cc',
        'browser/download/download_file_unittest.cc',
        'browser/download/download_manager_unittest.cc',
        'browser/download/download_request_infobar_delegate_unittest.cc',
        'browser/download/download_request_limiter_unittest.cc',
        'browser/download/download_status_updater_unittest.cc',
        'browser/download/download_util_unittest.cc',
        'browser/download/mock_download_manager.h',
        'browser/download/save_package_unittest.cc',
        'browser/enumerate_modules_model_unittest_win.cc',
        'browser/extensions/convert_user_script_unittest.cc',
        'browser/extensions/convert_web_app_unittest.cc',
        'browser/extensions/default_apps_unittest.cc',
        'browser/extensions/extension_event_router_forwarder_unittest.cc',
        'browser/extensions/extension_icon_manager_unittest.cc',
        'browser/extensions/extension_info_map_unittest.cc',
        'browser/extensions/extension_menu_manager_unittest.cc',
        'browser/extensions/extension_pref_value_map_unittest.cc',
        'browser/extensions/extension_prefs_unittest.cc',
        'browser/extensions/extension_process_manager_unittest.cc',
        'browser/extensions/extension_omnibox_unittest.cc',
        'browser/extensions/extension_service_unittest.cc',
        'browser/extensions/extension_service_unittest.h',
        'browser/extensions/extension_special_storage_policy_unittest.cc',
        'browser/extensions/extension_ui_unittest.cc',
        'browser/extensions/extension_updater_unittest.cc',
        'browser/extensions/extension_webnavigation_unittest.cc',
        'browser/extensions/extensions_quota_service_unittest.cc',
        'browser/extensions/external_policy_extension_loader_unittest.cc',
        'browser/extensions/file_reader_unittest.cc',
        'browser/extensions/image_loading_tracker_unittest.cc',
        'browser/extensions/key_identifier_conversion_views_unittest.cc',
        'browser/extensions/sandboxed_extension_unpacker_unittest.cc',
        'browser/extensions/user_script_listener_unittest.cc',
        'browser/extensions/user_script_master_unittest.cc',
        'browser/first_run/first_run_unittest.cc',
        'browser/geolocation/device_data_provider_unittest.cc',
        'browser/geolocation/fake_access_token_store.cc',
        'browser/geolocation/fake_access_token_store.h',
        'browser/geolocation/gateway_data_provider_common_unittest.cc',
        'browser/geolocation/geolocation_content_settings_map_unittest.cc',
        'browser/geolocation/geolocation_exceptions_table_model_unittest.cc',
        'browser/geolocation/geolocation_provider_unittest.cc',
        'browser/geolocation/geolocation_permission_context_unittest.cc',
        'browser/geolocation/geolocation_settings_state_unittest.cc',
        'browser/geolocation/gps_location_provider_unittest_linux.cc',
        'browser/geolocation/location_arbitrator_unittest.cc',
        'browser/geolocation/network_location_provider_unittest.cc',
        'browser/geolocation/wifi_data_provider_common_unittest.cc',
        'browser/geolocation/wifi_data_provider_unittest_chromeos.cc',
        'browser/geolocation/wifi_data_provider_unittest_win.cc',
        'browser/geolocation/win7_location_api_unittest_win.cc',
        'browser/geolocation/win7_location_provider_unittest_win.cc',
        'browser/global_keyboard_shortcuts_mac_unittest.mm',
        'browser/google/google_update_settings_unittest.cc',
        'browser/google/google_url_tracker_unittest.cc',
        'browser/ui/gtk/bookmark_bar_gtk_unittest.cc',
        'browser/ui/gtk/bookmark_editor_gtk_unittest.cc',
        'browser/ui/gtk/bookmark_utils_gtk_unittest.cc',
        'browser/ui/gtk/gtk_chrome_shrinkable_hbox_unittest.cc',
        'browser/ui/gtk/gtk_expanded_container_unittest.cc',
        'browser/ui/gtk/gtk_theme_provider_unittest.cc',
        'browser/ui/gtk/keyword_editor_view_unittest.cc',
        'browser/ui/gtk/options/content_exceptions_window_gtk_unittest.cc',
        'browser/ui/gtk/options/cookies_view_unittest.cc',
        'browser/ui/gtk/options/languages_page_gtk_unittest.cc',
        'browser/ui/gtk/reload_button_gtk_unittest.cc',
        'browser/ui/gtk/status_icons/status_tray_gtk_unittest.cc',
        'browser/ui/gtk/tabs/tab_renderer_gtk_unittest.cc',
        'browser/history/expire_history_backend_unittest.cc',
        'browser/history/history_backend_unittest.cc',
        'browser/history/history_querying_unittest.cc',
        'browser/history/history_types_unittest.cc',
        'browser/history/history_unittest.cc',
        'browser/history/in_memory_url_index_unittest.cc',
        'browser/history/query_parser_unittest.cc',
        'browser/history/snippet_unittest.cc',
        'browser/history/starred_url_database_unittest.cc',
        'browser/history/text_database_manager_unittest.cc',
        'browser/history/text_database_unittest.cc',
        'browser/history/thumbnail_database_unittest.cc',
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
        'browser/instant/instant_loader_manager_unittest.cc',
        'browser/instant/promo_counter_unittest.cc',
        'browser/keychain_mock_mac.cc',
        'browser/keychain_mock_mac.h',
        'browser/mach_broker_mac_unittest.cc',
        'browser/metrics/metrics_log_unittest.cc',
        'browser/metrics/metrics_response_unittest.cc',
        'browser/metrics/metrics_service_unittest.cc',
        'browser/metrics/thread_watcher_unittest.cc',
        'browser/mock_plugin_exceptions_table_model.cc',
        'browser/mock_plugin_exceptions_table_model.h',
        'browser/net/connection_tester_unittest.cc',
        'browser/net/gaia/token_service_unittest.cc',
        'browser/net/gaia/token_service_unittest.h',
        'browser/net/chrome_net_log_unittest.cc',
        'browser/net/load_timing_observer_unittest.cc',
        'browser/net/passive_log_collector_unittest.cc',
        'browser/net/predictor_unittest.cc',
        'browser/net/pref_proxy_config_service_unittest.cc',
        'browser/net/resolve_proxy_msg_helper_unittest.cc',
        'browser/net/sqlite_persistent_cookie_store_unittest.cc',
        'browser/net/url_fixer_upper_unittest.cc',
        'browser/net/url_info_unittest.cc',
        'browser/notifications/desktop_notification_service_unittest.cc',
        'browser/notifications/desktop_notifications_unittest.h',
        'browser/notifications/desktop_notifications_unittest.cc',
        'browser/notifications/notification_exceptions_table_model_unittest.cc',
        'browser/notifications/notifications_prefs_cache_unittest.cc',
        'browser/parsers/metadata_parser_filebase_unittest.cc',
        'browser/password_manager/encryptor_unittest.cc',
        'browser/password_manager/encryptor_password_mac_unittest.cc',
        'browser/password_manager/login_database_unittest.cc',
        'browser/password_manager/password_form_data.cc',
        'browser/password_manager/password_form_manager_unittest.cc',
        'browser/password_manager/password_manager_unittest.cc',
        'browser/password_manager/password_store_default_unittest.cc',
        'browser/password_manager/password_store_mac_unittest.cc',
        'browser/password_manager/password_store_win_unittest.cc',
        'browser/plugin_exceptions_table_model_unittest.cc',
        'browser/policy/asynchronous_policy_loader_unittest.cc',
        'browser/policy/asynchronous_policy_provider_unittest.cc',
        'browser/policy/asynchronous_policy_test_base.cc',
        'browser/policy/asynchronous_policy_test_base.h',
        'browser/policy/cloud_policy_cache_unittest.cc',
        'browser/policy/cloud_policy_controller_unittest.cc',
        'browser/policy/config_dir_policy_provider_unittest.cc',
        'browser/policy/configuration_policy_pref_store_unittest.cc',
        'browser/policy/configuration_policy_provider_mac_unittest.cc',
        'browser/policy/configuration_policy_provider_win_unittest.cc',
        'browser/policy/device_token_fetcher_unittest.cc',
        'browser/policy/file_based_policy_provider_unittest.cc',
        'browser/policy/device_management_backend_mock.cc',
        'browser/policy/device_management_backend_mock.h',
        'browser/policy/device_management_service_unittest.cc',
        'browser/policy/managed_prefs_banner_base_unittest.cc',
        'browser/policy/mock_configuration_policy_provider.cc',
        'browser/policy/mock_configuration_policy_provider.h',
        'browser/policy/mock_configuration_policy_store.cc',
        'browser/policy/mock_configuration_policy_store.h',
        'browser/policy/mock_device_management_backend.cc',
        'browser/policy/mock_device_management_backend.h',
        'browser/policy/policy_map_unittest.cc',
        'browser/policy/policy_path_parser_unittest.cc',
        'browser/preferences_mock_mac.cc',
        'browser/preferences_mock_mac.h',
        'browser/prefs/command_line_pref_store_unittest.cc',
        'browser/prefs/overlay_persistent_pref_store_unittest.cc',
        'browser/prefs/pref_change_registrar_unittest.cc',
        'browser/prefs/pref_member_unittest.cc',
        'browser/prefs/pref_notifier_impl_unittest.cc',
        'browser/prefs/pref_service_unittest.cc',
        'browser/prefs/pref_set_observer_unittest.cc',
        'browser/prefs/pref_value_map_unittest.cc',
        'browser/prefs/pref_value_store_unittest.cc',
        'browser/prefs/proxy_config_dictionary_unittest.cc',
        'browser/prefs/proxy_prefs_unittest.cc',
        'browser/prefs/session_startup_pref_unittest.cc',
        'browser/prerender/prerender_manager_unittest.cc',
        'browser/prerender/prerender_resource_handler_unittest.cc',
        'browser/printing/cloud_print/cloud_print_setup_source_unittest.cc',
        'browser/printing/print_dialog_cloud_unittest.cc',
        'browser/printing/print_job_unittest.cc',
        'browser/printing/print_preview_tab_controller_unittest.cc',
        'browser/process_info_snapshot_mac_unittest.cc',
        'browser/process_singleton_mac_unittest.cc',
        'browser/profiles/profile_manager_unittest.cc',
        'browser/remoting/directory_add_request_unittest.cc',
        'browser/renderer_host/gtk_im_context_wrapper_unittest.cc',
        'browser/renderer_host/gtk_key_bindings_handler_unittest.cc',
        'browser/renderer_host/render_widget_host_view_mac_unittest.mm',
        'browser/renderer_host/web_cache_manager_unittest.cc',
        'browser/resources_util_unittest.cc',
        'browser/rlz/rlz_unittest.cc',
        'browser/safe_browsing/bloom_filter_unittest.cc',
        'browser/safe_browsing/chunk_range_unittest.cc',
        'browser/safe_browsing/client_side_detection_host_unittest.cc',
        'browser/safe_browsing/client_side_detection_service_unittest.cc',
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
        'browser/search_engines/search_host_to_urls_map_unittest.cc',
        'browser/search_engines/search_provider_install_data_unittest.cc',
        'browser/search_engines/template_url_fetcher_unittest.cc',
        'browser/search_engines/template_url_model_test_util.cc',
        'browser/search_engines/template_url_model_test_util.h',
        'browser/search_engines/template_url_model_unittest.cc',
        'browser/search_engines/template_url_parser_unittest.cc',
        'browser/search_engines/template_url_prepopulate_data_unittest.cc',
        'browser/search_engines/template_url_scraper_unittest.cc',
        'browser/search_engines/template_url_unittest.cc',
        'browser/sessions/session_backend_unittest.cc',
        'browser/sessions/session_service_unittest.cc',
        'browser/shell_integration_unittest.cc',
        'browser/speech/endpointer/endpointer_unittest.cc',
        'browser/speech/speech_input_bubble_controller_unittest.cc',
        'browser/speech/speech_recognition_request_unittest.cc',
        'browser/speech/speech_recognizer_unittest.cc',
        'browser/spellchecker_platform_engine_unittest.cc',
        'browser/ssl/ssl_host_state_unittest.cc',
        'browser/status_icons/status_icon_unittest.cc',
        'browser/status_icons/status_tray_unittest.cc',
        'browser/sync/abstract_profile_sync_service_test.cc',
        'browser/sync/abstract_profile_sync_service_test.h',
        'browser/sync/engine/read_node_mock.cc',
        'browser/sync/engine/read_node_mock.h',
        'browser/sync/glue/autofill_data_type_controller_unittest.cc',
        'browser/sync/glue/autofill_model_associator_unittest.cc',
        'browser/sync/glue/autofill_profile_model_associator_unittest.cc',
        'browser/sync/glue/bookmark_data_type_controller_unittest.cc',
        'browser/sync/glue/change_processor_mock.cc',
        'browser/sync/glue/change_processor_mock.h',
        'browser/sync/glue/data_type_controller_mock.cc',
        'browser/sync/glue/data_type_controller_mock.h',
        'browser/sync/glue/data_type_manager_impl_unittest.cc',
        'browser/sync/glue/data_type_manager_impl2_unittest.cc',
        'browser/sync/glue/data_type_manager_mock.cc',
        'browser/sync/glue/data_type_manager_mock.h',
        'browser/sync/glue/database_model_worker_unittest.cc',
        'browser/sync/glue/extension_data_unittest.cc',
        'browser/sync/glue/extension_data_type_controller_unittest.cc',
        'browser/sync/glue/extension_util_unittest.cc',
        'browser/sync/glue/http_bridge_unittest.cc',
        'browser/sync/glue/model_associator_mock.cc',
        'browser/sync/glue/model_associator_mock.h',
        'browser/sync/glue/preference_data_type_controller_unittest.cc',
        'browser/sync/glue/preference_model_associator_unittest.cc',
        'browser/sync/glue/session_model_associator_unittest.cc',
        'browser/sync/glue/sync_backend_host_mock.cc',
        'browser/sync/glue/sync_backend_host_mock.h',
        'browser/sync/glue/theme_data_type_controller_unittest.cc',
        'browser/sync/glue/theme_util_unittest.cc',
        'browser/sync/glue/typed_url_model_associator_unittest.cc',
        'browser/sync/glue/ui_model_worker_unittest.cc',
        'browser/sync/profile_sync_factory_impl_unittest.cc',
        'browser/sync/profile_sync_factory_mock.cc',
        'browser/sync/profile_sync_factory_mock.h',
        'browser/sync/profile_sync_service_autofill_unittest.cc',
        'browser/sync/profile_sync_service_password_unittest.cc',
        'browser/sync/profile_sync_service_preference_unittest.cc',
        'browser/sync/profile_sync_service_session_unittest.cc',
        'browser/sync/profile_sync_service_startup_unittest.cc',
        'browser/sync/profile_sync_service_typed_url_unittest.cc',
        'browser/sync/profile_sync_service_unittest.cc',
        'browser/sync/profile_sync_test_util.cc',
        'browser/sync/profile_sync_test_util.h',
        'browser/sync/signin_manager_unittest.cc',
        'browser/sync/sync_setup_wizard_unittest.cc',
        'browser/sync/sync_ui_util_mac_unittest.mm',
        'browser/sync/sync_ui_util_unittest.cc',
        'browser/sync/test_profile_sync_service.h',
        'browser/sync/test_profile_sync_service.cc',
        'browser/sync/util/cryptographer_unittest.cc',
        'browser/sync/util/nigori_unittest.cc',
        'browser/tab_contents/tab_specific_content_settings_unittest.cc',
        'browser/tab_contents/thumbnail_generator_unittest.cc',
        'browser/tab_contents/web_contents_unittest.cc',
        'browser/tabs/pinned_tab_codec_unittest.cc',
        'browser/tabs/tab_strip_model_unittest.cc',
        'browser/task_manager/task_manager_unittest.cc',
        'browser/themes/browser_theme_pack_unittest.cc',
        'browser/themes/browser_theme_provider_unittest.cc',
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
        'browser/ui/cocoa/applescript/bookmark_item_applescript_unittest.mm',
        'browser/ui/cocoa/applescript/bookmark_folder_applescript_unittest.mm',
        'browser/ui/cocoa/background_gradient_view_unittest.mm',
        'browser/ui/cocoa/background_tile_view_unittest.mm',
        'browser/ui/cocoa/base_view_unittest.mm',
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
        'browser/ui/cocoa/browser_frame_view_unittest.mm',
        'browser/ui/cocoa/browser_window_cocoa_unittest.mm',
        'browser/ui/cocoa/browser_window_controller_unittest.mm',
        'browser/ui/cocoa/bubble_view_unittest.mm',
        'browser/ui/cocoa/bug_report_window_controller_unittest.mm',
        'browser/ui/cocoa/chrome_browser_window_unittest.mm',
        'browser/ui/cocoa/chrome_event_processing_window_unittest.mm',
        'browser/ui/cocoa/clear_browsing_data_controller_unittest.mm',
        'browser/ui/cocoa/clickhold_button_cell_unittest.mm',
        'browser/ui/cocoa/cocoa_test_helper.h',
        'browser/ui/cocoa/cocoa_test_helper.mm',
        'browser/ui/cocoa/command_observer_bridge_unittest.mm',
        'browser/ui/cocoa/confirm_quit_panel_controller_unittest.mm',
        'browser/ui/cocoa/content_settings/collected_cookies_mac_unittest.mm',
        'browser/ui/cocoa/content_settings/content_setting_bubble_cocoa_unittest.mm',
        'browser/ui/cocoa/content_settings/cookie_details_unittest.mm',
        'browser/ui/cocoa/content_settings/cookie_details_view_controller_unittest.mm',
        'browser/ui/cocoa/content_settings/simple_content_exceptions_window_controller_unittest.mm',
        'browser/ui/cocoa/download/download_item_button_unittest.mm',
        'browser/ui/cocoa/download/download_shelf_mac_unittest.mm',
        'browser/ui/cocoa/download/download_shelf_view_unittest.mm',
        'browser/ui/cocoa/download/download_util_mac_unittest.mm',
        'browser/ui/cocoa/draggable_button_unittest.mm',
        'browser/ui/cocoa/event_utils_unittest.mm',
        'browser/ui/cocoa/extensions/browser_actions_container_view_unittest.mm',
        'browser/ui/cocoa/extensions/chevron_menu_button_unittest.mm',
        'browser/ui/cocoa/extensions/extension_install_prompt_controller_unittest.mm',
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
        'browser/ui/cocoa/gradient_button_cell_unittest.mm',
        'browser/ui/cocoa/history_menu_bridge_unittest.mm',
        'browser/ui/cocoa/history_menu_cocoa_controller_unittest.mm',
        'browser/ui/cocoa/hover_image_button_unittest.mm',
        'browser/ui/cocoa/html_dialog_window_controller_unittest.mm',
        'browser/ui/cocoa/hung_renderer_controller_unittest.mm',
        'browser/ui/cocoa/hyperlink_button_cell_unittest.mm',
        'browser/ui/cocoa/image_utils_unittest.mm',
        'browser/ui/cocoa/importer/import_settings_dialog_unittest.mm',
        'browser/ui/cocoa/info_bubble_view_unittest.mm',
        'browser/ui/cocoa/info_bubble_window_unittest.mm',
        'browser/ui/cocoa/infobars/infobar_container_controller_unittest.mm',
        'browser/ui/cocoa/infobars/infobar_controller_unittest.mm',
        'browser/ui/cocoa/infobars/infobar_gradient_view_unittest.mm',
        'browser/ui/cocoa/instant_confirm_window_controller_unittest.mm',
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
        'browser/ui/cocoa/objc_method_swizzle_unittest.mm',
        'browser/ui/cocoa/options/content_exceptions_window_controller_unittest.mm',
        'browser/ui/cocoa/options/content_settings_dialog_controller_unittest.mm',
        'browser/ui/cocoa/options/cookies_window_controller_unittest.mm',
        'browser/ui/cocoa/options/custom_home_pages_model_unittest.mm',
        'browser/ui/cocoa/options/edit_search_engine_cocoa_controller_unittest.mm',
        'browser/ui/cocoa/options/font_language_settings_controller_unittest.mm',
        'browser/ui/cocoa/options/keyword_editor_cocoa_controller_unittest.mm',
        'browser/ui/cocoa/options/preferences_window_controller_unittest.mm',
        'browser/ui/cocoa/options/search_engine_list_model_unittest.mm',
        'browser/ui/cocoa/page_info_bubble_controller_unittest.mm',
        'browser/ui/cocoa/rwhvm_editcommand_helper_unittest.mm',
        'browser/ui/cocoa/status_bubble_mac_unittest.mm',
        'browser/ui/cocoa/status_icons/status_icon_mac_unittest.mm',
        'browser/ui/cocoa/styled_text_field_cell_unittest.mm',
        'browser/ui/cocoa/styled_text_field_test_helper.h',
        'browser/ui/cocoa/styled_text_field_test_helper.mm',
        'browser/ui/cocoa/styled_text_field_unittest.mm',
        'browser/ui/cocoa/tab_contents/previewable_contents_controller_unittest.mm',
        'browser/ui/cocoa/tab_contents/sad_tab_controller_unittest.mm',
        'browser/ui/cocoa/tab_contents/sad_tab_view_unittest.mm',
        'browser/ui/cocoa/tab_contents/web_drop_target_unittest.mm',
        'browser/ui/cocoa/tabs/side_tab_strip_view_unittest.mm',
        'browser/ui/cocoa/tabs/tab_controller_unittest.mm',
        'browser/ui/cocoa/tabs/tab_strip_controller_unittest.mm',
        'browser/ui/cocoa/tabs/tab_strip_view_unittest.mm',
        'browser/ui/cocoa/tabs/tab_view_unittest.mm',
        'browser/ui/cocoa/tabs/throbber_view_unittest.mm',
        'browser/ui/cocoa/tab_view_picker_table_unittest.mm',
        'browser/ui/cocoa/table_model_array_controller_unittest.mm',
        'browser/ui/cocoa/table_row_nsimage_cache_unittest.mm',
        'browser/ui/cocoa/tabpose_window_unittest.mm',
        'browser/ui/cocoa/task_manager_mac_unittest.mm',
        'browser/ui/cocoa/test_event_utils.h',
        'browser/ui/cocoa/test_event_utils.mm',
        'browser/ui/cocoa/toolbar/reload_button_unittest.mm',
        'browser/ui/cocoa/toolbar/toolbar_controller_unittest.mm',
        'browser/ui/cocoa/toolbar/toolbar_view_unittest.mm',
        'browser/ui/cocoa/tracking_area_unittest.mm',
        'browser/ui/cocoa/translate/translate_infobar_unittest.mm',
        'browser/ui/cocoa/vertical_gradient_view_unittest.mm',
        'browser/ui/cocoa/view_resizer_pong.h',
        'browser/ui/cocoa/view_resizer_pong.mm',
        'browser/ui/cocoa/window_size_autosaver_unittest.mm',
        'browser/ui/cocoa/wrench_menu/menu_tracked_button_unittest.mm',
        'browser/ui/cocoa/wrench_menu/menu_tracked_root_view_unittest.mm',
        'browser/ui/cocoa/wrench_menu/wrench_menu_button_cell_unittest.mm',
        'browser/ui/cocoa/wrench_menu/wrench_menu_controller_unittest.mm',
        'browser/ui/find_bar/find_backend_unittest.cc',
        'browser/ui/login/login_prompt_unittest.cc',
        'browser/ui/search_engines/keyword_editor_controller_unittest.cc',
        'browser/ui/tabs/dock_info_unittest.cc',
        'browser/ui/tabs/tab_menu_model_unittest.cc',
        'browser/ui/tests/ui_gfx_image_unittest.cc',
        'browser/ui/tests/ui_gfx_image_unittest.mm',
        'browser/ui/toolbar/back_forward_menu_model_unittest.cc',
        'browser/ui/toolbar/encoding_menu_controller_unittest.cc',
        'browser/ui/toolbar/wrench_menu_model_unittest.cc',
        'browser/ui/views/accessibility_event_router_views_unittest.cc',
        'browser/ui/views/bookmark_bar_view_unittest.cc',
        'browser/ui/views/bookmark_context_menu_test.cc',
        'browser/ui/views/bookmark_editor_view_unittest.cc',
        'browser/ui/views/extensions/browser_action_drag_data_unittest.cc',
        'browser/ui/views/generic_info_view_unittest.cc',
        'browser/ui/views/info_bubble_unittest.cc',
        'browser/ui/views/reload_button_unittest.cc',
        'browser/ui/views/shell_dialogs_win_unittest.cc',
        'browser/ui/views/status_icons/status_tray_win_unittest.cc',
        'browser/ui/window_sizer_unittest.cc',
        'browser/ui/window_snapshot/window_snapshot_mac_unittest.mm',
        'browser/user_style_sheet_watcher_unittest.cc',
        'browser/visitedlink/visitedlink_unittest.cc',
        'browser/web_applications/web_app_unittest.cc',
        'browser/webdata/web_data_service_test_util.h',
        'browser/webdata/web_data_service_unittest.cc',
        'browser/webdata/web_database_unittest.cc',
        'browser/webui/html_dialog_tab_contents_delegate_unittest.cc',
        'browser/webui/options/language_options_handler_unittest.cc',
        'browser/webui/print_preview_ui_html_source_unittest.cc',
        'browser/webui/shown_sections_handler_unittest.cc',
        'browser/webui/sync_internals_ui_unittest.cc',
        'browser/webui/theme_source_unittest.cc',
        'browser/web_resource/promo_resource_service_unittest.cc',
        'common/bzip2_unittest.cc',
        'common/child_process_logging_mac_unittest.mm',
        'common/chrome_paths_unittest.cc',
        'common/common_param_traits_unittest.cc',
        'common/content_settings_helper_unittest.cc',
        'common/deprecated/event_sys_unittest.cc',
        'common/desktop_notifications/active_notification_tracker_unittest.cc',
        'common/extensions/extension_action_unittest.cc',
        'common/extensions/extension_extent_unittest.cc',
        'common/extensions/extension_file_util_unittest.cc',
        'common/extensions/extension_icon_set_unittest.cc',
        'common/extensions/extension_l10n_util_unittest.cc',
        'common/extensions/extension_localization_peer_unittest.cc',
        'common/extensions/extension_manifests_unittest.cc',
        'common/extensions/extension_message_bundle_unittest.cc',
        'common/extensions/extension_resource_unittest.cc',
        'common/extensions/extension_unittest.cc',
        'common/extensions/extension_set_unittest.cc',
        'common/extensions/extension_unpacker_unittest.cc',
        'common/extensions/update_manifest_unittest.cc',
        'common/extensions/url_pattern_unittest.cc',
        'common/extensions/user_script_unittest.cc',
        'common/font_descriptor_mac_unittest.mm',
        'common/gpu_feature_flags_unittest.cc',
        'common/gpu_info_unittest.cc',
        'common/gpu_messages_unittest.cc',
        'common/guid_unittest.cc',
        'common/important_file_writer_unittest.cc',
        'common/json_pref_store_unittest.cc',
        'common/json_schema_validator_unittest_base.cc',
        'common/json_schema_validator_unittest_base.h',
        'common/json_schema_validator_unittest.cc',
        'common/json_value_serializer_unittest.cc',
        'common/mru_cache_unittest.cc',
        'common/multi_process_lock_unittest.cc',
        'common/net/gaia/gaia_auth_fetcher_unittest.cc',
        'common/net/gaia/gaia_auth_fetcher_unittest.h',
        'common/net/gaia/gaia_authenticator_unittest.cc',
        'common/net/gaia/google_service_auth_error_unittest.cc',
        'common/net/url_fetcher_unittest.cc',
        'common/notification_service_unittest.cc',
        'common/process_watcher_unittest.cc',
        'common/property_bag_unittest.cc',
        'common/render_messages_unittest.cc',
        'common/resource_dispatcher_unittest.cc',
        'common/sandbox_mac_diraccess_unittest.mm',
        'common/sandbox_mac_fontloading_unittest.mm',
        'common/sandbox_mac_unittest_helper.h',
        'common/sandbox_mac_unittest_helper.mm',
        'common/sandbox_mac_system_access_unittest.mm',
        'common/service_process_util_unittest.cc',
        'common/switch_utils_unittest.cc',
        'common/thumbnail_score_unittest.cc',
        'common/time_format_unittest.cc',
        'common/web_apps_unittest.cc',
        'common/worker_thread_ticker_unittest.cc',
        'common/zip_unittest.cc',
        'gpu/gpu_idirect3d9_mock_win.cc',
        'gpu/gpu_idirect3d9_mock_win.h',
        'gpu/gpu_info_collector_unittest.cc',
        'gpu/gpu_info_unittest_win.cc',
        'gpu/gpu_video_decoder_unittest.cc',
        'renderer/audio_message_filter_unittest.cc',
        'renderer/extensions/extension_api_json_validity_unittest.cc',
        'renderer/extensions/json_schema_unittest.cc',
        'renderer/gpu_video_decoder_host_unittest.cc',
        'renderer/media/audio_renderer_impl_unittest.cc',
        'renderer/net/predictor_queue_unittest.cc',
        'renderer/net/renderer_predictor_unittest.cc',
        'renderer/paint_aggregator_unittest.cc',
        'renderer/render_process_unittest.cc',
        'renderer/render_thread_unittest.cc',
        'renderer/render_widget_unittest.cc',
        'renderer/renderer_about_handler_unittest.cc',
        'renderer/renderer_main_unittest.cc',
        'renderer/safe_browsing/features_unittest.cc',
        'renderer/safe_browsing/phishing_term_feature_extractor_unittest.cc',
        'renderer/safe_browsing/phishing_url_feature_extractor_unittest.cc',
        'renderer/safe_browsing/scorer_unittest.cc',
        'renderer/spellchecker/spellcheck_unittest.cc',
        'renderer/spellchecker/spellcheck_provider_unittest.cc',
        'renderer/spellchecker/spellcheck_worditerator_unittest.cc',
        'service/cloud_print/cloud_print_helpers_unittest.cc',
        'service/cloud_print/cloud_print_url_fetcher_unittest.cc',
        'service/service_process_unittest.cc',
        'test/browser_with_test_window_test.cc',
        'test/browser_with_test_window_test.h',
        'test/data/resource.rc',
        'test/file_test_utils.cc',
        'test/file_test_utils.h',
        'test/menu_model_test.cc',
        'test/menu_model_test.h',
        'test/render_view_test.cc',
        'test/render_view_test.h',
        'test/sync/test_http_bridge_factory.cc',
        'test/sync/test_http_bridge_factory.h',
        'test/test_notification_tracker.cc',
        'test/test_notification_tracker.h',
        'test/v8_unit_test.cc',
        'test/v8_unit_test.h',
        'tools/convert_dict/convert_dict_unittest.cc',
        '../content/browser/appcache/chrome_appcache_service_unittest.cc',
        '../content/browser/browser_thread_unittest.cc',
        '../content/browser/child_process_security_policy_unittest.cc',
        '../content/browser/device_orientation/provider_unittest.cc',
        '../content/browser/gpu_blacklist_unittest.cc',
        '../content/browser/host_zoom_map_unittest.cc',
        '../content/browser/in_process_webkit/webkit_context_unittest.cc',
        '../content/browser/in_process_webkit/webkit_thread_unittest.cc',
        '../content/browser/plugin_service_unittest.cc',
        '../content/browser/renderer_host/audio_renderer_host_unittest.cc',
        '../content/browser/renderer_host/render_view_host_unittest.cc',
        '../content/browser/renderer_host/render_widget_host_unittest.cc',
        '../content/browser/renderer_host/resource_dispatcher_host_unittest.cc',
        '../content/browser/renderer_host/resource_queue_unittest.cc',
        '../content/browser/site_instance_unittest.cc',
        '../content/browser/tab_contents/navigation_controller_unittest.cc',
        '../content/browser/tab_contents/navigation_entry_unittest.cc',
        '../content/browser/tab_contents/render_view_host_manager_unittest.cc',
        '../content/browser/webui/web_ui_unittest.cc',
        '../testing/gtest_mac_unittest.mm',
        '../third_party/cld/encodings/compact_lang_det/compact_lang_det_unittest_small.cc',
        '../webkit/fileapi/file_system_dir_url_request_job_unittest.cc',
        '../webkit/fileapi/file_system_url_request_job_unittest.cc',
      ],
      'conditions': [
        ['touchui==1', {
          'sources!': [
             'browser/renderer_host/gtk_im_context_wrapper_unittest.cc',
          ],
        }, { # else: touchui == 0
          'sources/': [
            ['exclude', '^browser/chromeos/webui/login/'],
          ],
        }],
        ['chromeos==1', {
          'sources/': [
            # TODO(thestig) Enable PrintPreviewUIHTMLSource tests on CrOS when
            # print preview is enabled on CrOS.
            ['exclude', 'browser/notifications/desktop_notifications_unittest.cc'],
            ['exclude', 'browser/webui/print_preview_ui_html_source_unittest.cc'],
          ],
        }, { # else: chromeos == 0
          'sources/': [
            ['exclude', '^browser/chromeos/'],
          ],
        }],
        ['OS=="linux"', {
          'conditions': [
            ['gcc_version==44', {
              # Avoid gcc 4.4 strict aliasing issues in stl_tree.h when
              # building mru_cache_unittest.cc.
              'cflags': [
                '-fno-strict-aliasing',
              ],
            }],
            ['selinux==0', {
              'dependencies': [
                '../sandbox/sandbox.gyp:*',
              ],
            }],
            ['toolkit_views==1', {
              'sources!': [
                 'browser/autocomplete/autocomplete_popup_view_gtk_unittest.cc',
                 'browser/ui/gtk/bookmark_bar_gtk_unittest.cc',
                 'browser/ui/gtk/bookmark_editor_gtk_unittest.cc',
                 'browser/ui/gtk/gtk_chrome_shrinkable_hbox_unittest.cc',
                 'browser/ui/gtk/gtk_expanded_container_unittest.cc',
                 'browser/ui/gtk/gtk_theme_provider_unittest.cc',
                 'browser/ui/gtk/options/cookies_view_unittest.cc',
                 'browser/ui/gtk/options/languages_page_gtk_unittest.cc',
                 'browser/ui/gtk/reload_button_gtk_unittest.cc',
                 'browser/ui/gtk/status_icons/status_tray_gtk_unittest.cc',
              ],
            }],
          ],
          'dependencies': [
            '../build/linux/system.gyp:gtk',
            '../build/linux/system.gyp:nss',
            '../tools/xdisplaycheck/xdisplaycheck.gyp:xdisplaycheck',
          ],
          'sources!': [
            'browser/printing/print_job_unittest.cc',
          ],
        }, { # else: OS != "linux"
          'sources!': [
            'browser/ui/gtk/tabs/tab_renderer_gtk_unittest.cc',
            'browser/renderer_host/gtk_key_bindings_handler_unittest.cc',
            '../views/focus/accelerator_handler_gtk_unittest.cc',
          ],
        }],
        ['OS=="linux" or OS=="freebsd"', {
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
            'convert_dict_lib',
            '../third_party/hunspell/hunspell.gyp:hunspell',
          ],
          'sources!': [
            'browser/spellchecker_platform_engine_unittest.cc',
          ],
        }],
        ['OS=="win"', {
          'dependencies': [
            'chrome_dll_version',
            'installer_util_strings',
            '../third_party/iaccessible2/iaccessible2.gyp:iaccessible2',
            '../third_party/isimpledom/isimpledom.gyp:isimpledom',
            'test_chrome_plugin',  # run time dependency
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
            '<(SHARED_INTERMEDIATE_DIR)/app/app_resources/app_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/autofill_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/browser_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/common_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/renderer_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/theme_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome_dll_version/chrome_dll_version.rc',
            '<(SHARED_INTERMEDIATE_DIR)/net/net_resources.rc',
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
            'browser/bookmarks/bookmark_codec_unittest.cc',
            'browser/bookmarks/bookmark_node_data_unittest.cc',
            'browser/chrome_plugin_unittest.cc',
            'browser/extensions/extension_process_manager_unittest.cc',
            'browser/login_prompt_unittest.cc',
            'browser/rlz/rlz_unittest.cc',
            'browser/search_engines/template_url_scraper_unittest.cc',
            'browser/ui/views/bookmark_editor_view_unittest.cc',
            'browser/ui/views/extensions/browser_action_drag_data_unittest.cc',
            'browser/ui/views/find_bar_host_unittest.cc',
            'browser/ui/views/generic_info_view_unittest.cc',
            'browser/ui/views/keyword_editor_view_unittest.cc',
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
      ],
    },
    {
      # Executable that runs each browser test in a new process.
      'target_name': 'browser_tests',
      'type': 'executable',
      'msvs_guid': 'D7589D0D-304E-4589-85A4-153B7D84B07F',
      'dependencies': [
        'browser',
        'browser/sync/protocol/sync_proto.gyp:sync_proto_cpp',
        'chrome',
        'chrome_resources',
        'chrome_strings',
        'profile_import',
        'renderer',
        'test_support_common',
        '../app/app.gyp:app_base',
        '../base/base.gyp:base',
        '../base/base.gyp:base_i18n',
        '../base/base.gyp:test_support_base',
        '../net/net.gyp:net_test_support',
        '../skia/skia.gyp:skia',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        '../third_party/cld/cld.gyp:cld',
        '../third_party/icu/icu.gyp:icui18n',
        '../third_party/icu/icu.gyp:icuuc',
        # Runtime dependencies
        'chrome_mesa',
      ],
      'include_dirs': [
        '..',
      ],
      'defines': [ 'ALLOW_IN_PROC_BROWSER_TEST' ],
      'sources': [
        'app/breakpad_mac_stubs.mm',
        'app/chrome_command_ids.h',
        'app/chrome_dll.rc',
        'app/chrome_dll_resource.h',
        'app/chrome_dll_version.rc.version',
        'browser/accessibility/renderer_accessibility_browsertest.cc',
        'browser/autocomplete/autocomplete_browsertest.cc',
        'browser/autofill/form_structure_browsertest.cc',
        'browser/browser_browsertest.cc',
        'browser/browsing_data_database_helper_browsertest.cc',
        'browser/browsing_data_helper_browsertest.h',
        'browser/browsing_data_indexed_db_helper_browsertest.cc',
        'browser/browsing_data_local_storage_helper_browsertest.cc',
        'browser/chromeos/cros/cros_in_process_browser_test.cc',
        'browser/chromeos/cros/cros_in_process_browser_test.h',
        'browser/chromeos/cros/cros_mock.cc',
        'browser/chromeos/cros/cros_mock.h',
        'browser/chromeos/cros/mock_cros_library.h',
        'browser/chromeos/cros/mock_cryptohome_library.h',
        'browser/chromeos/cros/mock_keyboard_library.h',
        'browser/chromeos/cros/mock_input_method_library.h',
        'browser/chromeos/cros/mock_mount_library.cc',
        'browser/chromeos/cros/mock_mount_library.h',
        'browser/chromeos/cros/mock_network_library.h',
        'browser/chromeos/cros/mock_power_library.h',
        'browser/chromeos/cros/mock_speech_synthesis_library.h',
        'browser/chromeos/cros/mock_system_library.h',
        'browser/chromeos/cros/mock_touchpad_library.h',
        'browser/chromeos/login/existing_user_controller_browsertest.cc',
        'browser/chromeos/login/login_browsertest.cc',
        'browser/chromeos/login/mock_authenticator.h',
        'browser/chromeos/login/network_screen_browsertest.cc',
        'browser/chromeos/login/screen_locker_browsertest.cc',
        'browser/chromeos/login/screen_locker_tester.cc',
        'browser/chromeos/login/screen_locker_tester.h',
        'browser/chromeos/login/update_screen_browsertest.cc',
        'browser/chromeos/login/wizard_controller_browsertest.cc',
        'browser/chromeos/login/wizard_in_process_browser_test.cc',
        'browser/chromeos/login/wizard_in_process_browser_test.h',
        'browser/chromeos/network_state_notifier_browsertest.cc',
        'browser/chromeos/notifications/notification_browsertest.cc',
        'browser/chromeos/panels/panel_browsertest.cc',
        'browser/chromeos/status/clock_menu_button_browsertest.cc',
        'browser/chromeos/status/input_method_menu_button_browsertest.cc',
        'browser/chromeos/status/power_menu_button_browsertest.cc',
        'browser/chromeos/tab_closeable_state_watcher_browsertest.cc',
        'browser/chromeos/update_browsertest.cc',
        'browser/crash_recovery_browsertest.cc',
        'browser/download/download_browsertest.cc',
        'browser/download/save_page_browsertest.cc',
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
        'browser/extensions/content_script_apitest.cc',
        'browser/extensions/convert_web_app_browsertest.cc',
        'browser/extensions/cross_origin_xhr_apitest.cc',
        'browser/extensions/crx_installer_browsertest.cc',
        'browser/extensions/events_apitest.cc',
        'browser/extensions/extension_devtools_browsertest.cc',
        'browser/extensions/extension_devtools_browsertest.h',
        'browser/extensions/extension_devtools_browsertests.cc',
        'browser/extensions/execute_script_apitest.cc',
        'browser/extensions/extension_apitest.cc',
        'browser/extensions/extension_apitest.h',
        'browser/extensions/extension_bookmarks_apitest.cc',
        'browser/extensions/extension_bookmarks_unittest.cc',
        'browser/extensions/extension_bookmark_manager_apitest.cc',
        'browser/extensions/extension_browsertest.cc',
        'browser/extensions/extension_browsertest.h',
        'browser/extensions/extension_browsertests_misc.cc',
        'browser/extensions/extension_clipboard_apitest.cc',
        'browser/extensions/extension_content_settings_apitest.cc',
        'browser/extensions/extension_context_menu_apitest.cc',
        'browser/extensions/extension_context_menu_browsertest.cc',
        'browser/extensions/extension_cookies_apitest.cc',
        'browser/extensions/extension_cookies_unittest.cc',
        'browser/extensions/extension_crash_recovery_browsertest.cc',
        'browser/extensions/extension_fileapi_apitest.cc',
        'browser/extensions/extension_gallery_install_apitest.cc',
        'browser/extensions/extension_geolocation_apitest.cc',
        'browser/extensions/extension_get_views_apitest.cc',
        'browser/extensions/extension_history_apitest.cc',
        'browser/extensions/extension_idle_apitest.cc',
        'browser/extensions/extension_i18n_apitest.cc',
        'browser/extensions/extension_incognito_apitest.cc',
        'browser/extensions/extension_infobar_apitest.cc',
        'browser/extensions/extension_input_apitest.cc',
        'browser/extensions/extension_install_ui_browsertest.cc',
        'browser/extensions/extension_javascript_url_apitest.cc',
        'browser/extensions/extension_management_api_browsertest.cc',
        'browser/extensions/extension_management_apitest.cc',
        'browser/extensions/extension_management_browsertest.cc',
        'browser/extensions/extension_messages_apitest.cc',
        'browser/extensions/extension_messages_browsertest.cc',
        'browser/extensions/extension_metrics_apitest.cc',
        'browser/extensions/extension_module_apitest.cc',
        'browser/extensions/extension_omnibox_apitest.cc',
        'browser/extensions/extension_override_apitest.cc',
        'browser/extensions/extension_proxy_apitest.cc',
        'browser/extensions/extension_processes_apitest.cc',
        'browser/extensions/extension_resource_request_policy_apitest.cc',
        'browser/extensions/extension_rlz_apitest.cc',
        'browser/extensions/extension_sidebar_apitest.cc',
        'browser/extensions/extension_startup_browsertest.cc',
        'browser/extensions/extension_storage_apitest.cc',
        'browser/extensions/extension_tabs_apitest.cc',
        'browser/extensions/extension_test_message_listener.cc',
        'browser/extensions/extension_test_message_listener.h',
        'browser/extensions/extension_toolbar_model_browsertest.cc',
        'browser/extensions/extension_tts_apitest.cc',
        'browser/extensions/extension_webglbackground_apitest.cc',
        'browser/extensions/extension_webnavigation_apitest.cc',
        'browser/extensions/extension_webrequest_apitest.cc',
        'browser/extensions/extension_websocket_apitest.cc',
        'browser/extensions/extension_webstore_private_browsertest.cc',
        'browser/extensions/notifications_apitest.cc',
        'browser/extensions/page_action_apitest.cc',
        'browser/extensions/permissions_apitest.cc',
        'browser/extensions/stubs_apitest.cc',
        'browser/extensions/window_open_apitest.cc',
        'browser/file_path_watcher/file_path_watcher_browsertest.cc',
        'browser/first_run/first_run_browsertest.cc',
        'browser/geolocation/access_token_store_browsertest.cc',
        'browser/geolocation/geolocation_browsertest.cc',
        'browser/ui/gtk/view_id_util_browsertest.cc',
        'browser/history/history_browsertest.cc',
        'browser/idbbindingutilities_browsertest.cc',
        'browser/net/cookie_policy_browsertest.cc',
        'browser/net/ftp_browsertest.cc',
        'browser/plugin_data_remover_browsertest.cc',
        'browser/policy/device_management_backend_mock.cc',
        'browser/policy/device_management_backend_mock.h',
        'browser/policy/device_management_service_browsertest.cc',
        'browser/popup_blocker_browsertest.cc',
        'browser/prerender/prerender_browsertest.cc',
        'browser/printing/print_dialog_cloud_uitest.cc',
        'browser/renderer_host/web_cache_manager_browsertest.cc',
        'browser/safe_browsing/safe_browsing_blocking_page_test.cc',
        'browser/safe_browsing/safe_browsing_service_browsertest.cc',
        'browser/service/service_process_control_browsertest.cc',
        'browser/sessions/session_restore_browsertest.cc',
        'browser/sessions/tab_restore_service_browsertest.cc',
        'browser/speech/speech_input_browsertest.cc',
        'browser/speech/speech_input_bubble_browsertest.cc',
        'browser/ssl/ssl_browser_tests.cc',
        'browser/task_manager/task_manager_browsertest.cc',
        'browser/translate/translate_manager_browsertest.cc',
        'browser/ui/browser_init_browsertest.cc',
        'browser/ui/browser_navigator_browsertest.cc',
        'browser/ui/browser_navigator_browsertest.h',
        'browser/ui/browser_navigator_browsertest_chromeos.cc',
        'browser/ui/cocoa/view_id_util_browsertest.mm',
        'browser/ui/cocoa/applescript/browsercrapplication+applescript_test.mm',
        'browser/ui/cocoa/applescript/window_applescript_test.mm',
        'browser/ui/find_bar/find_bar_host_browsertest.cc',
        'browser/ui/login/login_prompt_browsertest.cc',
        'browser/ui/views/browser_actions_container_browsertest.cc',
        'browser/ui/views/dom_view_browsertest.cc',
        'browser/ui/views/html_dialog_view_browsertest.cc',
        'browser/webui/file_browse_browsertest.cc',
        'browser/webui/mediaplayer_browsertest.cc',
        'browser/webui/settings_browsertest.cc',
        'renderer/autofill/form_autocomplete_browsertest.cc',
        'renderer/autofill/form_manager_browsertest.cc',
        'renderer/autofill/password_autofill_manager_unittest.cc',
        'renderer/page_click_tracker_browsertest.cc',
        'renderer/pepper_devices_browsertest.cc',
        'renderer/render_view_browsertest.cc',
        'renderer/render_view_browsertest_mac.mm',
        'renderer/render_widget_browsertest.cc',
        'renderer/render_widget_browsertest.h',
        'renderer/safe_browsing/malware_dom_details_browsertest.cc',
        'renderer/safe_browsing/phishing_classifier_browsertest.cc',
        'renderer/safe_browsing/phishing_classifier_delegate_browsertest.cc',
        'renderer/safe_browsing/phishing_dom_feature_extractor_browsertest.cc',
        'renderer/safe_browsing/phishing_thumbnailer_browsertest.cc',
        'renderer/safe_browsing/render_view_fake_resources_test.cc',
        'renderer/safe_browsing/render_view_fake_resources_test.h',
        'renderer/translate_helper_browsertest.cc',
        'test/automation/dom_automation_browsertest.cc',
        'test/gpu/gpu_browsertest.cc',
        'test/out_of_proc_test_runner.cc',
        'test/render_view_test.cc',
        'test/render_view_test.h',
        '../content/browser/child_process_security_policy_browsertest.cc',
        '../content/browser/device_orientation/device_orientation_browsertest.cc',
        '../content/browser/in_process_webkit/dom_storage_browsertest.cc',
        '../content/browser/in_process_webkit/indexed_db_browsertest.cc',
        '../content/browser/plugin_service_browsertest.cc',
        '../content/browser/renderer_host/render_process_host_browsertest.cc',
        '../content/browser/renderer_host/render_view_host_browsertest.cc',
        '../content/browser/renderer_host/render_view_host_manager_browsertest.cc',
        '../content/browser/webui/web_ui_browsertest.cc',
        '../content/browser/webui/web_ui_browsertest.h',
        '../content/browser/webui/web_ui_handler_browsertest.cc',
        '../content/browser/webui/web_ui_handler_browsertest.h',
      ],
      'conditions': [
        ['chromeos==0', {
          'sources/': [
            ['exclude', '^browser/chromeos'],
          ],
          'sources!': [
            'browser/webui/file_browse_browsertest.cc',
            'browser/webui/mediaplayer_browsertest.cc',
          ],
        }],
        ['toolkit_views==0', {
          'sources!': [
            'browser/extensions/extension_input_apitest.cc',
          ],
        }],
        ['internal_pdf', {
          'sources': [
            'test/plugin/pdf_browsertest.cc',
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
            '<(SHARED_INTERMEDIATE_DIR)/app/app_resources/app_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/autofill_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/browser_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/common_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome_dll_version/chrome_dll_version.rc',
            '<(SHARED_INTERMEDIATE_DIR)/net/net_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/renderer_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/theme_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/webkit_chromium_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/webkit_resources.rc',
            # TODO(alekseys): port sidebar to linux/mac.
            'browser/sidebar/sidebar_browsertest.cc',
          ],
          'include_dirs': [
            '<(DEPTH)/third_party/wtl/include',
          ],
          'dependencies': [
            'chrome_dll_version',
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
            'app/chrome_dll_version.rc.version',
            'browser/extensions/extension_rlz_apitest.cc',
            # http://crbug.com/15101 These tests fail on Linux and Mac.
            'browser/renderer_host/web_cache_manager_browsertest.cc',
            '../content/browser/child_process_security_policy_browsertest.cc',
            '../content/browser/renderer_host/render_view_host_manager_browsertest.cc',
          ],
        }],
        ['OS=="linux"', {
          'dependencies': [
            '../build/linux/system.gyp:gtk',
            '../build/linux/system.gyp:nss',
            '../tools/xdisplaycheck/xdisplaycheck.gyp:xdisplaycheck',
          ],
          'sources': [
            # TODO(estade): port to win/mac.
            'browser/webui/constrained_html_ui_browsertest.cc',
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
            'renderer/external_popup_menu_unittest.cc',
            'browser/spellcheck_message_filter_browsertest.cc',
          ],
        }, { # else: OS != "mac"
          'sources!': [
            'browser/extensions/browser_action_test_util_mac.mm',
          ],
        }],
        ['OS=="linux" or OS=="freebsd"', {
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
            'browser/webui/constrained_html_ui_browsertest.cc',
          ],
        }, { # else: toolkit_views == 0
          'sources!': [
            'browser/extensions/browser_action_test_util_views.cc',
            'browser/ui/views/browser_actions_container_browsertest.cc',
            'browser/ui/views/dom_view_browsertest.cc',
            'browser/ui/views/html_dialog_view_browsertest.cc',
          ],
        }],
        ['target_arch!="arm"', {
          'dependencies': [
            # run time dependency
            '../webkit/webkit.gyp:copy_npapi_test_plugin',
          ],
        }],
      ],  # conditions
    },  # target browser_tests
    {
      # Executable that runs safebrowsing test in a new process.
      'target_name': 'safe_browsing_tests',
      'type': 'executable',
      'msvs_guid': 'BBF2BC2F-7CD8-463E-BE88-CB81AAD92BFE',
      'dependencies': [
        'chrome',
        'test_support_common',
        '../app/app.gyp:app_resources',
        '../base/base.gyp:base',
        '../net/net.gyp:net_test_support',
        '../skia/skia.gyp:skia',
        '../testing/gtest.gyp:gtest',
        # This is the safebrowsing test server.
        '../third_party/safe_browsing/safe_browsing.gyp:safe_browsing',
      ],
      'include_dirs': [
        '..',
      ],
      'defines': [ 'ALLOW_IN_PROC_BROWSER_TEST' ],
      'sources': [
        'app/chrome_dll.rc',
        'browser/safe_browsing/safe_browsing_test.cc',
        'test/out_of_proc_test_runner.cc',
      ],
      'conditions': [
        ['OS=="win"', {
          'dependencies': [
            'chrome_dll_version',
            'installer_util_strings',
            '../sandbox/sandbox.gyp:sandbox',
          ],
          'sources': [
            '<(SHARED_INTERMEDIATE_DIR)/app/app_resources/app_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/autofill_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/browser_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/common_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome_dll_version/chrome_dll_version.rc',
            '<(SHARED_INTERMEDIATE_DIR)/net/net_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/renderer_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/theme_resources.rc',
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
      'target_name': 'startup_tests',
      'type': 'executable',
      'msvs_guid': 'D3E6C0FD-54C7-4FF2-9AE1-72F2DAFD820C',
      'dependencies': [
        'chrome',
        'browser',
        'common',
        'chrome_resources',
        'chrome_strings',
        'test_support_ui',
        '../app/app.gyp:app_base',
        '../base/base.gyp:base',
        '../skia/skia.gyp:skia',
        '../testing/gtest.gyp:gtest',
      ],
      'sources': [
        'test/startup/feature_startup_test.cc',
        'test/startup/shutdown_test.cc',
        'test/startup/startup_test.cc',
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
        ['OS=="linux"', {
          'dependencies': [
            '../build/linux/system.gyp:gtk',
            '../tools/xdisplaycheck/xdisplaycheck.gyp:xdisplaycheck',
          ],
        }],
        ['OS=="mac"', {
          # See the comment in this section of the unit_tests target for an
          # explanation (crbug.com/43791 - libwebcore.a is too large to mmap).
          'dependencies+++': [
            '../third_party/WebKit/Source/WebCore/WebCore.gyp/WebCore.gyp:webcore',
          ],
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
        },],
        ['OS=="linux" or OS=="freebsd"', {
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
      # To run the tests from page_load_test.cc on Linux, we need to:
      #
      #   a) Build with Breakpad (GYP_DEFINES="linux_chromium_breakpad=1")
      #   b) Run with CHROME_HEADLESS=1 to generate crash dumps.
      #   c) Strip the binary if it's a debug build. (binary may be over 2GB)
      'target_name': 'reliability_tests',
      'type': 'executable',
      'msvs_guid': '8A3E1774-1DE9-445C-982D-3EE37C8A752A',
      'dependencies': [
        'browser',
        'chrome',
        'test_support_common',
        'test_support_ui',
        'theme_resources',
        '../skia/skia.gyp:skia',
        '../testing/gtest.gyp:gtest',
        '../third_party/WebKit/Source/WebKit/chromium/WebKit.gyp:webkit',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'test/reliability/page_load_test.cc',
        'test/reliability/page_load_test.h',
        'test/reliability/reliability_test_suite.h',
        'test/reliability/run_all_unittests.cc',
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
        ['OS=="linux"', {
          'dependencies': [
            '../build/linux/system.gyp:gtk',
          ],
        },],
      ],
    },
    {
      'target_name': 'page_cycler_tests',
      'type': 'executable',
      'msvs_guid': 'C9E0BD1D-B175-4A91-8380-3FDC81FAB9D7',
      'dependencies': [
        'chrome',
        'chrome_resources',
        'chrome_strings',
        'debugger',
        'test_support_common',
        'test_support_ui',
        '../base/base.gyp:base',
        '../skia/skia.gyp:skia',
        '../testing/gtest.gyp:gtest',
      ],
      'sources': [
        'test/page_cycler/page_cycler_test.cc',
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
        ['OS=="linux"', {
          'dependencies': [
            '../build/linux/system.gyp:gtk',
            '../tools/xdisplaycheck/xdisplaycheck.gyp:xdisplaycheck',
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
      'target_name': 'tab_switching_test',
      'type': 'executable',
      'msvs_guid': 'A34770EA-A574-43E8-9327-F79C04770E98',
      'run_as': {
        'action': ['$(TargetPath)', '--gtest_print_time'],
      },
      'dependencies': [
        'chrome',
        'debugger',
        'test_support_common',
        'test_support_ui',
        'theme_resources',
        '../base/base.gyp:base',
        '../skia/skia.gyp:skia',
        '../testing/gtest.gyp:gtest',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'test/tab_switching/tab_switching_test.cc',
      ],
      'conditions': [
        ['OS=="linux"', {
          'dependencies': [
            '../build/linux/system.gyp:gtk',
            '../tools/xdisplaycheck/xdisplaycheck.gyp:xdisplaycheck',
          ],
        }],
        ['OS=="win" and win_use_allocator_shim==1', {
          'dependencies': [
            '<(allocator_target)',
          ],
        },],
      ],
    },
    {
      'target_name': 'memory_test',
      'type': 'executable',
      'msvs_guid': 'A5F831FD-9B9C-4FEF-9FBA-554817B734CE',
      'dependencies': [
        'chrome',
        'debugger',
        'test_support_common',
        'test_support_ui',
        'theme_resources',
        '../base/base.gyp:base',
        '../skia/skia.gyp:skia',
        '../testing/gtest.gyp:gtest',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'test/memory_test/memory_test.cc',
      ],
      'conditions': [
        ['OS=="linux"', {
          'dependencies': [
            '../build/linux/system.gyp:gtk',
            '../tools/xdisplaycheck/xdisplaycheck.gyp:xdisplaycheck',
          ],
        }],
      ],
    },
    {
      'target_name': 'url_fetch_test',
      'type': 'executable',
      'msvs_guid': '7EFD0C91-198E-4043-9E71-4A4C7879B929',
      'dependencies': [
        'chrome',
        'debugger',
        'test_support_common',
        'test_support_ui',
        'theme_resources',
        '../base/base.gyp:base',
        '../net/net.gyp:net',
        '../skia/skia.gyp:skia',
        '../testing/gtest.gyp:gtest',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'test/url_fetch_test/url_fetch_test.cc',
      ],
      'conditions': [
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
        }], # OS="win"
      ], # conditions
    },
    {
      'target_name': 'sync_unit_tests',
      'type': 'executable',
      'sources': [
        '<(protoc_out_dir)/chrome/browser/sync/protocol/test.pb.cc',
        'app/breakpad_mac_stubs.mm',
        'browser/sync/engine/apply_updates_command_unittest.cc',
        'browser/sync/engine/clear_data_command_unittest.cc',
        'browser/sync/engine/cleanup_disabled_types_command_unittest.cc',
        'browser/sync/engine/download_updates_command_unittest.cc',
        'browser/sync/engine/mock_model_safe_workers.cc',
        'browser/sync/engine/mock_model_safe_workers.h',
        'browser/sync/engine/process_commit_response_command_unittest.cc',
        'browser/sync/engine/syncapi_unittest.cc',
        'browser/sync/engine/syncer_proto_util_unittest.cc',
        'browser/sync/engine/syncer_thread_unittest.cc',
        'browser/sync/engine/syncer_thread2_unittest.cc',
        'browser/sync/engine/syncer_unittest.cc',
        'browser/sync/engine/syncproto_unittest.cc',
        'browser/sync/engine/syncapi_mock.h',
        'browser/sync/engine/verify_updates_command_unittest.cc',
        'browser/sync/js_arg_list_unittest.cc',
        'browser/sync/js_event_handler_list_unittest.cc',
        'browser/sync/js_sync_manager_observer_unittest.cc',
        'browser/sync/notifier/cache_invalidation_packet_handler_unittest.cc',
        'browser/sync/notifier/chrome_invalidation_client_unittest.cc',
        'browser/sync/notifier/chrome_system_resources_unittest.cc',
        'browser/sync/notifier/registration_manager_unittest.cc',
        'browser/sync/notifier/server_notifier_thread_unittest.cc',
        'browser/sync/profile_sync_factory_mock.h',
        'browser/sync/protocol/proto_enum_conversions_unittest.cc',
        'browser/sync/protocol/proto_value_conversions_unittest.cc',
        'browser/sync/sessions/ordered_commit_set_unittest.cc',
        'browser/sync/sessions/session_state_unittest.cc',
        'browser/sync/sessions/status_controller_unittest.cc',
        'browser/sync/sessions/sync_session_unittest.cc',
        'browser/sync/sessions/test_util.cc',
        'browser/sync/sessions/test_util.h',
        'browser/sync/syncable/directory_backing_store_unittest.cc',
        'browser/sync/syncable/model_type_unittest.cc',
        'browser/sync/syncable/nigori_util_unittest.cc',
        'browser/sync/syncable/syncable_id_unittest.cc',
        'browser/sync/syncable/syncable_unittest.cc',
        'browser/sync/util/channel_unittest.cc',
        'browser/sync/util/crypto_helpers_unittest.cc',
        'browser/sync/util/data_encryption_unittest.cc',
        'browser/sync/util/extensions_activity_monitor_unittest.cc',
        'browser/sync/util/protobuf_unittest.cc',
        'browser/sync/util/user_settings_unittest.cc',
        'test/file_test_utils.cc',
        'test/sync/engine/mock_connection_manager.cc',
        'test/sync/engine/mock_connection_manager.h',
        'test/sync/engine/mock_gaia_authenticator.cc',
        'test/sync/engine/mock_gaia_authenticator.h',
        'test/sync/engine/mock_gaia_authenticator_unittest.cc',
        'test/sync/engine/syncer_command_test.h',
        'test/sync/engine/test_directory_setter_upper.cc',
        'test/sync/engine/test_directory_setter_upper.h',
        'test/sync/engine/test_id_factory.h',
        'test/sync/engine/test_syncable_utils.cc',
        'test/sync/engine/test_syncable_utils.h',
        'test/sync/sessions/test_scoped_session_event_listener.h',
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
        'browser/sync/protocol/sync_proto.gyp:sync_proto_cpp',
        'common',
        'debugger',
        '../jingle/jingle.gyp:notifier_test_util',
        '../skia/skia.gyp:skia',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        '../third_party/bzip2/bzip2.gyp:bzip2',
        '../third_party/libjingle/libjingle.gyp:libjingle',
        'profile_import',
        'syncapi',
        'sync_notifier',
        'test_support_common',
        'test_support_sync',
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
        ['OS=="linux"', {
          'dependencies': [
            '../build/linux/system.gyp:gtk',
            '../build/linux/system.gyp:nss',
            'packed_resources'
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
            'packed_extra_resources',
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
        'browser/sync/protocol/sync_proto.gyp:sync_proto_cpp',
        'chrome',
        'chrome_resources',
        'common',
        'profile_import',
        'renderer',
        'chrome_strings',
        'test_support_common',
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
      'defines': [ 'ALLOW_IN_PROC_BROWSER_TEST' ],
      'sources': [
        'app/chrome_command_ids.h',
        'app/chrome_dll.rc',
        'app/chrome_dll_resource.h',
        'app/chrome_dll_version.rc.version',
        'browser/password_manager/password_form_data.cc',
        'browser/sessions/session_backend.cc',
        'browser/sync/glue/session_model_associator.cc',
        'test/out_of_proc_test_runner.cc',
        'test/live_sync/bookmark_model_verifier.cc',
        'test/live_sync/bookmark_model_verifier.h',
        'test/live_sync/live_autofill_sync_test.cc',
        'test/live_sync/live_autofill_sync_test.h',
        'test/live_sync/live_bookmarks_sync_test.cc',
        'test/live_sync/live_bookmarks_sync_test.h',
        'test/live_sync/live_extensions_sync_test_base.cc',
        'test/live_sync/live_extensions_sync_test_base.h',
        'test/live_sync/live_extensions_sync_test.cc',
        'test/live_sync/live_extensions_sync_test.h',
        'test/live_sync/live_passwords_sync_test.h',
        'test/live_sync/live_preferences_sync_test.h',
        'test/live_sync/live_sessions_sync_test.cc',
        'test/live_sync/live_sessions_sync_test.h',
        'test/live_sync/live_themes_sync_test.cc',
        'test/live_sync/live_themes_sync_test.h',
        'test/live_sync/live_sync_test.cc',
        'test/live_sync/live_sync_test.h',
        'test/live_sync/many_client_live_bookmarks_sync_test.cc',
        'test/live_sync/many_client_live_passwords_sync_test.cc',
        'test/live_sync/many_client_live_preferences_sync_test.cc',
        'test/live_sync/multiple_client_live_bookmarks_sync_test.cc',
        'test/live_sync/multiple_client_live_passwords_sync_test.cc',
        'test/live_sync/multiple_client_live_preferences_sync_test.cc',
        'test/live_sync/multiple_client_live_sessions_sync_test.cc',
        'test/live_sync/single_client_live_bookmarks_sync_test.cc',
        'test/live_sync/single_client_live_extensions_sync_test.cc',
        'test/live_sync/single_client_live_passwords_sync_test.cc',
        'test/live_sync/single_client_live_preferences_sync_test.cc',
        'test/live_sync/single_client_live_sessions_sync_test.cc',
        'test/live_sync/single_client_live_themes_sync_test.cc',
        'test/live_sync/two_client_live_autofill_sync_test.cc',
        'test/live_sync/two_client_live_bookmarks_sync_test.cc',
        'test/live_sync/two_client_live_extensions_sync_test.cc',
        'test/live_sync/two_client_live_preferences_sync_test.cc',
        'test/live_sync/two_client_live_passwords_sync_test.cc',
        'test/live_sync/two_client_live_sessions_sync_test.cc',
        'test/live_sync/two_client_live_themes_sync_test.cc',
        'test/test_notification_tracker.cc',
        'test/test_notification_tracker.h',
        'test/ui_test_utils_linux.cc',
        'test/ui_test_utils_mac.mm',
        'test/ui_test_utils_win.cc',
        'test/data/resource.rc',
      ],
      'conditions': [
        # Plugin code.
        ['OS=="linux" or OS=="win"', {
          'dependencies': [
            'plugin',
           ],
          'export_dependent_settings': [
            'plugin',
          ],
        }],
        ['OS=="linux"', {
           'dependencies': [
             '../build/linux/system.gyp:gtk',
             '../build/linux/system.gyp:nss',
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
            '<(SHARED_INTERMEDIATE_DIR)/app/app_resources/app_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/autofill_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/browser_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/common_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome_dll_version/chrome_dll_version.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/theme_resources.rc',
          ],
          'include_dirs': [
            '<(DEPTH)/third_party/wtl/include',
          ],
          'dependencies': [
            'chrome_dll_version',
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
            'app/chrome_dll_version.rc.version',
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
      'msvs_guid': '3D3BB86C-F284-4911-BAEB-12C6EFA09A01',
      'dependencies': [
        'browser',
        'chrome',
        'chrome_resources',
        'chrome_strings',
        'renderer',
        'test_support_common',
        '../app/app.gyp:app_base',
        '../base/base.gyp:base',
        '../base/base.gyp:test_support_base',
        '../net/net.gyp:net_test_support',
        '../skia/skia.gyp:skia',
        '../testing/gtest.gyp:gtest',
        '../third_party/icu/icu.gyp:icui18n',
        '../third_party/icu/icu.gyp:icuuc',
        # Runtime dependencies
        'chrome_mesa',
      ],
      'include_dirs': [
        '..',
      ],
      'defines': [ 'ALLOW_IN_PROC_BROWSER_TEST' ],
      'sources': [
        'test/gpu/gpu_pixel_browsertest.cc',
        'test/out_of_proc_test_runner.cc',
      ],
      'conditions': [
        ['OS=="win"', {
          'dependencies': [
            'chrome_dll_version',
            'installer_util_strings',
            '../sandbox/sandbox.gyp:sandbox',
          ],
          'include_dirs': [
            '<(DEPTH)/third_party/wtl/include',
          ],
          'sources': [
            'app/chrome_dll.rc',
            'app/chrome_dll_resource.h',
            'app/chrome_dll_version.rc.version',
            '<(SHARED_INTERMEDIATE_DIR)/app/app_resources/app_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/autofill_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/browser_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/common_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/renderer_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/theme_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome_dll_version/chrome_dll_version.rc',
            '<(SHARED_INTERMEDIATE_DIR)/net/net_resources.rc',
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
        ['OS=="linux"', {
           'dependencies': [
             '../build/linux/system.gyp:gtk',
             '../build/linux/system.gyp:nss',
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
      'msvs_guid': 'A1CAA831-C507-4B2E-87F3-AEC63C9907F9',
      'dependencies': [
        'chrome',
        'chrome_resources',
        'chrome_strings',
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
        'test/plugin/plugin_test.cpp',
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
    ['OS!="mac"', {
      'targets': [
        {
          'target_name': 'perf_tests',
          'type': 'executable',
          'msvs_guid': '9055E088-25C6-47FD-87D5-D9DD9FD75C9F',
          'dependencies': [
            'browser',
            'common',
            'renderer',
            'chrome_gpu',
            'chrome_resources',
            'chrome_strings',
            '../app/app.gyp:app_base',
            '../base/base.gyp:base',
            '../base/base.gyp:test_support_base',
            '../base/base.gyp:test_support_perf',
            '../skia/skia.gyp:skia',
            '../testing/gtest.gyp:gtest',
            '../webkit/support/webkit_support.gyp:glue',
          ],
          'sources': [
            'browser/safe_browsing/filter_false_positive_perftest.cc',
            'browser/visitedlink/visitedlink_perftest.cc',
            'common/json_value_serializer_perftest.cc',
            'test/perf/perftests.cc',
            'test/perf/url_parse_perftest.cc',
          ],
          'conditions': [
            ['OS=="linux"', {
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
          'msvs_guid': '2E969AE9-7B12-4EDB-8E8B-48C7AE7BE357',
          'dependencies': [
            'test_support_common',
            'browser',
            'renderer',
            'syncapi',
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
          'msvs_guid': 'E750512D-FC7C-4C98-BF04-0A0DAF882055',
          'include_dirs': [
            '..',
          ],
          'sources': [
            'test/injection_test_dll.h',
            'test/security_tests/ipc_security_tests.cc',
            'test/security_tests/ipc_security_tests.h',
            'test/security_tests/security_tests.cc',
            '../sandbox/tests/validation_tests/commands.cc',
            '../sandbox/tests/validation_tests/commands.h',
          ],
        },
        # Extra 64-bit DLL for windows
        {
          'target_name': 'nacl_security_tests64',
          'type': 'shared_library',
          'configurations': {
            'Common_Base': {
              'msvs_target_platform': 'x64',
            },
          },
          'include_dirs': [
            '..'
          ],
          'sources': [
            '../sandbox/tests/validation_tests/commands.cc',
            '../sandbox/tests/validation_tests/commands.h',
            '../sandbox/tests/common/controller.h',
            'test/nacl_security_tests/nacl_security_tests_win.h',
            'test/nacl_security_tests/nacl_security_tests_win.cc',
          ],
        },
        {
          'target_name': 'selenium_tests',
          'type': 'executable',
          'msvs_guid': 'E3749617-BA3D-4230-B54C-B758E56D9FA5',
          'dependencies': [
            'chrome_resources',
            'chrome_strings',
            'test_support_common',
            'test_support_ui',
            '../skia/skia.gyp:skia',
            '../testing/gtest.gyp:gtest',
          ],
          'include_dirs': [
            '..',
            '<(DEPTH)/third_party/wtl/include',
          ],
          'sources': [
            'test/selenium/selenium_test.cc',
          ],
          'conditions': [
            ['OS=="win" and win_use_allocator_shim==1', {
              'dependencies': [
                '<(allocator_target)',
              ],
            },],
          ],
        },
        {
          'target_name': 'test_chrome_plugin',
          'type': 'shared_library',
          'msvs_guid': '7F0A70F6-BE3F-4C19-B435-956AB8F30BA4',
          'dependencies': [
            '../base/base.gyp:base',
            '../build/temp_gyp/googleurl.gyp:googleurl',
          ],
          'include_dirs': [
            '..',
          ],
          'link_settings': {
            'libraries': [
              '-lwinmm.lib',
            ],
          },
          'sources': [
            'test/chrome_plugin/test_chrome_plugin.cc',
            'test/chrome_plugin/test_chrome_plugin.def',
            'test/chrome_plugin/test_chrome_plugin.h',
          ],
        },
      ]},  # 'targets'
    ],  # OS=="win"
    # If you change this condition, make sure you also change it in all.gyp
    # for the chromium_builder_qa target.
    ['OS=="mac" or OS=="win" or (OS=="linux" and target_arch==python_arch)', {
      'targets': [
        {
          # Documentation: http://dev.chromium.org/developers/testing/pyauto
          'target_name': 'pyautolib',
          'type': 'shared_library',
          'product_prefix': '_',
          'dependencies': [
            'chrome',
            'debugger',
            'syncapi',
            'test_support_common',
            'chrome_resources',
            'chrome_strings',
            'theme_resources',
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
            # Need a shared object named _pyautolib.so (not libpyautolib.dylib
            # that xcode would generate)
            # Change when gyp can support a platform-neutral way for this
            # (http://code.google.com/p/gyp/issues/detail?id=135)
            'EXECUTABLE_EXTENSION': 'so',
            # When generated, pyautolib_wrap.cc includes some swig support
            # files which, as of swig 1.3.31 that comes with 10.5 and 10.6,
            # may not compile cleanly at -Wall.
            'GCC_TREAT_WARNINGS_AS_ERRORS': 'NO',  # -Wno-error
          },
          'conditions': [
            ['OS=="linux"', {
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
              'actions': [
              {
                # _pyautolib.so gets created in lib.target dir.
                # Create a symlink from the product dir.
                'action_name': 'create_symlink',
                'inputs': [
                ],
                'outputs': [
                  '<(PRODUCT_DIR)/_pyautolib.so',
                ],
                'action': [ 'ln',
                            '-sf',
                            '<(PRODUCT_DIR)/lib.target/_pyautolib.so',
                            '<@(_outputs)',
                ],
                'message': 'Creating symlink: '
                           '<(PRODUCT_DIR)/lib.target/_pyautolib.so',
              }],  # actions
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
                '$(SDKROOT)/usr/include/python2.5',
              ],
              'link_settings': {
                'libraries': [
                  '$(SDKROOT)/usr/lib/libpython2.5.dylib',
                ],
              }
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
            '../app/app.gyp:app_unittests',
            '../base/base.gyp:base_unittests',
            # browser_tests's use of subprocesses chokes gcov on 10.6?
            # Disabling for now (enabled on linux/windows below).
            # 'browser_tests',
            '../ipc/ipc.gyp:ipc_tests',
            '../media/media.gyp:media_unittests',
            'nacl_sandbox_tests',
            'nacl_ui_tests',
            '../net/net.gyp:net_unittests',
            '../printing/printing.gyp:printing_unittests',
            '../remoting/remoting.gyp:remoting_unittests',
            # ui_tests seem unhappy on both Mac and Win when run under
            # coverage (all tests fail, often with a
            # "server_->WaitForInitialLoads()").  TODO(jrg):
            # investigate why.
            # 'ui_tests',
            'unit_tests',
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

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
