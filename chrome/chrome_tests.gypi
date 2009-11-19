# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
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
        'chrome_resources',
        'chrome_strings',
        'theme_resources',
        '../base/base.gyp:test_support_base',
        '../skia/skia.gyp:skia',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
      ],
      'export_dependent_settings': [
        'renderer',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        # The only thing used from browser is Browser::Type.
        'browser/browser.h',
        # TODO:  these should live here but are currently used by
        # production code code in libbrowser (above).
        #'browser/net/url_request_mock_http_job.cc',
        #'browser/net/url_request_mock_http_job.h',
        'browser/net/url_request_mock_net_error_job.cc',
        'browser/net/url_request_mock_net_error_job.h',
        'browser/renderer_host/mock_render_process_host.cc',
        'browser/renderer_host/mock_render_process_host.h',
        'browser/renderer_host/test/test_render_view_host.cc',
        'browser/renderer_host/test/test_render_view_host.h',
        'browser/tab_contents/test_tab_contents.cc',
        'browser/tab_contents/test_tab_contents.h',
        'common/ipc_test_sink.cc',
        'common/ipc_test_sink.h',
        'renderer/mock_keyboard.cc',
        'renderer/mock_keyboard.h',
        'renderer/mock_keyboard_driver_win.cc',
        'renderer/mock_keyboard_driver_win.h',
        'renderer/mock_printer.cc',
        'renderer/mock_printer.h',
        'renderer/mock_render_process.h',
        'renderer/mock_render_thread.cc',
        'renderer/mock_render_thread.h',
        'test/automation/autocomplete_edit_proxy.cc',
        'test/automation/autocomplete_edit_proxy.h',
        'test/automation/automation_constants.h',
        'test/automation/automation_handle_tracker.cc',
        'test/automation/automation_handle_tracker.h',
        'test/automation/automation_proxy.cc',
        'test/automation/automation_proxy.h',
        'test/automation/browser_proxy.cc',
        'test/automation/browser_proxy.h',
        'test/automation/tab_proxy.cc',
        'test/automation/tab_proxy.h',
        'test/automation/window_proxy.cc',
        'test/automation/window_proxy.h',
        'test/chrome_process_util.cc',
        'test/chrome_process_util.h',
        'test/chrome_process_util_linux.cc',
        'test/chrome_process_util_mac.cc',
        'test/chrome_process_util_win.cc',
        'test/test_browser_window.h',
        'test/testing_profile.cc',
        'test/testing_profile.h',
        'test/ui_test_utils.cc',
        'test/ui_test_utils.h',
        'test/ui_test_utils_linux.cc',
        'test/ui_test_utils_mac.cc',
        'test/ui_test_utils_win.cc',
      ],
      'conditions': [
        ['OS=="linux"', {
          'dependencies': [
            '../build/linux/system.gyp:gtk',
          ],
        }],
        ['OS=="win"', {
          'include_dirs': [
            'third_party/wtl/include',
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
        'test/testing_browser_process.h',
        'test/ui/javascript_test_util.cc',
        'test/ui/npapi_test_helper.cc',
        'test/ui/npapi_test_helper.h',
        'test/ui/run_all_unittests.cc',
        'test/ui/ui_layout_test.cc',
        'test/ui/ui_layout_test.h',
        'test/ui/ui_test.cc',
        'test/ui/ui_test.h',
        'test/ui/ui_test_suite.cc',
        'test/ui/ui_test_suite.h',
      ],
      'conditions': [
        ['OS=="linux"', {
          'dependencies': [
            '../build/linux/system.gyp:gtk',
          ],
        }],
        ['OS!="win"', {
          'sources!': [
            'test/ui/npapi_test_helper.cc',
          ],
        }],
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
        ['OS=="linux"', {
          'dependencies': [
            '../tools/xdisplaycheck/xdisplaycheck.gyp:xdisplaycheck',
          ],
        }],
        ['OS=="win"', {
          'include_dirs': [
            'third_party/wtl/include',
          ],
          'dependencies': [
            '../third_party/tcmalloc/tcmalloc.gyp:tcmalloc',
          ],
        }],
      ],
    },
    {
      'target_name': 'ui_tests',
      'type': 'executable',
      'msvs_guid': '76235B67-1C27-4627-8A33-4B2E1EF93EDE',
      'dependencies': [
        'chrome',
        'browser',
        'debugger',
        'common',
        'chrome_resources',
        'chrome_strings',
        'syncapi',
        'test_support_ui',
        '../base/base.gyp:base',
        '../net/net.gyp:net',
        '../build/temp_gyp/googleurl.gyp:googleurl',
        '../skia/skia.gyp:skia',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        '../third_party/icu/icu.gyp:icui18n',
        '../third_party/icu/icu.gyp:icuuc',
        '../third_party/libxml/libxml.gyp:libxml',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'app/chrome_main_uitest.cc',
        'browser/browser_encoding_uitest.cc',
        'browser/browser_uitest.cc',
        'browser/dom_ui/new_tab_ui_uitest.cc',
        'browser/download/download_uitest.cc',
        'browser/download/save_page_uitest.cc',
        'browser/errorpage_uitest.cc',
        'browser/extensions/extension_uitest.cc',
        'browser/history/redirect_uitest.cc',
        'browser/iframe_uitest.cc',
        'browser/images_uitest.cc',
        'browser/in_process_webkit/dom_storage_uitest.cc',
        'browser/locale_tests_uitest.cc',
        'browser/login_prompt_uitest.cc',
        'browser/media_uitest.cc',
        'browser/metrics/metrics_service_uitest.cc',
        'browser/notifications/notifications_uitest.cc',
        'browser/printing/printing_layout_uitest.cc',
        'browser/process_singleton_linux_uitest.cc',
        'browser/renderer_host/resource_dispatcher_host_uitest.cc',
        'browser/sanity_uitest.cc',
        'browser/session_history_uitest.cc',
        'browser/sessions/session_restore_uitest.cc',
        'browser/tab_contents/view_source_uitest.cc',
        'browser/tab_restore_uitest.cc',
        'browser/unload_uitest.cc',
        'browser/views/find_bar_host_uitest.cc',
        'common/logging_chrome_uitest.cc',
        'common/pref_service_uitest.cc',
        'test/automation/automation_proxy_uitest.cc',
        'test/automated_ui_tests/automated_ui_test_test.cc',
        'test/chrome_process_util_uitest.cc',
        'test/ui/dom_checker_uitest.cc',
        'test/ui/dromaeo_benchmark_uitest.cc',
        'test/ui/fast_shutdown_uitest.cc',
        'test/ui/history_uitest.cc',
        'test/ui/layout_plugin_uitest.cc',
        'test/ui/mouseleave_uitest.cc',
        'test/ui/npapi_uitest.cc',
        'test/ui/omnibox_uitest.cc',
        'test/ui/sandbox_uitests.cc',
        'test/ui/sunspider_uitest.cc',
        'test/ui/v8_benchmark_uitest.cc',
        'worker/worker_uitest.cc',
      ],
      'conditions': [
        ['OS=="linux"', {
          'dependencies': [
            '../build/linux/system.gyp:gtk',
            '../tools/xdisplaycheck/xdisplaycheck.gyp:xdisplaycheck',
          ],
          'sources!': [
            # TODO(port)
            'browser/login_prompt_uitest.cc',
            'test/ui/layout_plugin_uitest.cc',
          ],
        }, { # else: OS != "linux"
          'sources!': [
            'browser/process_singleton_linux_uitest.cc',
          ],
        }],
        ['OS=="linux" and toolkit_views==1', {
          'dependencies': [
            '../views/views.gyp:views',
          ],
        }],
        ['OS=="mac"', {
          'sources': [
            'app/keystone_glue.h',
            'app/keystone_glue.mm',
            'app/breakpad_mac_stubs.mm',
          ],
          'sources!': [
            # TODO(port)
            'app/chrome_main_uitest.cc',
            'browser/login_prompt_uitest.cc',
            'browser/metrics/metrics_service_uitest.cc',
            # leaves an extra window on screen after test completes.
            'browser/sessions/session_restore_uitest.cc',
            # puts up modal dialogs.
            'browser/unload_uitest.cc',
            'test/ui/fast_shutdown_uitest.cc',
            'test/ui/layout_plugin_uitest.cc',
            'test/ui/omnibox_uitest.cc',
          ],
        }],
        ['OS=="win"', {
          'include_dirs': [
            'third_party/wtl/include',
          ],
          'dependencies': [
            'crash_service',  # run time dependency
            'security_tests',  # run time dependency
            'test_support_common',
            '../google_update/google_update.gyp:google_update',
            '../views/views.gyp:views',
            # run time dependency
            '../webkit/tools/test_shell/test_shell.gyp:npapi_test_plugin',
            '../third_party/tcmalloc/tcmalloc.gyp:tcmalloc',
          ],
          'link_settings': {
            'libraries': [
              '-lOleAcc.lib',
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
        }, { # else: OS != "win"
          'sources!': [
            # TODO(port)?  (Most of these include windows.h or similar.)
            'browser/extensions/extension_uitest.cc',
            'browser/media_uitest.cc',
            'browser/printing/printing_layout_uitest.cc',
            'browser/views/find_bar_host_uitest.cc',
            'common/logging_chrome_uitest.cc',
            'test/ui/npapi_uitest.cc',
            'test/ui/sandbox_uitests.cc',
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
        'debugger',
        'common',
        'chrome_resources',
        'chrome_strings',
        'syncapi',
        'test_support_ui',
        '../base/base.gyp:base',
        '../net/net.gyp:net',
        '../build/temp_gyp/googleurl.gyp:googleurl',
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
      ],
      'conditions': [
        # TODO(gregoryd): This test will run on Windows only at first.
        # Refer to ui_tests target above if any problems appear when trying
        # to enable it on other platforms.
        ['OS=="win"', {
          'dependencies': [
            'crash_service',  # run time dependency
            'security_tests',  # run time dependency
            'test_support_common',
            '../google_update/google_update.gyp:google_update',
            '../views/views.gyp:views',
            # run time dependency
            '../webkit/tools/test_shell/test_shell.gyp:npapi_test_plugin',
            '../third_party/tcmalloc/tcmalloc.gyp:tcmalloc',
          ],
          'link_settings': {
            'libraries': [
              '-lOleAcc.lib',
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
      'msvs_guid': 'ECFC2BEC-9FC0-4AD9-9649-5F26793F65FC',
      'dependencies': [
        'chrome',
        'browser',
        'chrome_resources',
        'chrome_strings',
        'common',
        'debugger',
        'renderer',
        'syncapi',
        'test_support_unit',
        'utility',
        '../app/app.gyp:app_base',
        '../app/app.gyp:app_resources',
        '../ipc/ipc.gyp:ipc',
        '../net/net.gyp:net_resources',
        '../net/net.gyp:net_test_support',
        '../printing/printing.gyp:printing',
        '../webkit/webkit.gyp:webkit_resources',
        '../skia/skia.gyp:skia',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        '../third_party/bzip2/bzip2.gyp:bzip2',
        '../third_party/icu/icu.gyp:icui18n',
        '../third_party/icu/icu.gyp:icuuc',
        '../third_party/libxml/libxml.gyp:libxml',
        '../third_party/npapi/npapi.gyp:npapi',
        '../third_party/WebKit/WebKit/chromium/WebKit.gyp:webkit',
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
        # *NO* files in chrome/app have unit tests (except keystone_glue)!!!
        # It seems a waste to have an app_unittests target, so for now
        # I add keystone_glue.mm explicitly to this target.
        'app/keystone_glue.mm',
        'app/keystone_glue_unittest.mm',
        # All unittests in browser, common, and renderer.
        'browser/app_controller_mac_unittest.mm',
        'browser/autocomplete/autocomplete_edit_view_mac_unittest.mm',
        'browser/autocomplete/autocomplete_unittest.cc',
        'browser/autocomplete/autocomplete_popup_view_mac_unittest.mm',
        'browser/autocomplete/history_contents_provider_unittest.cc',
        'browser/autocomplete/history_url_provider_unittest.cc',
        'browser/autocomplete/keyword_provider_unittest.cc',
        'browser/autocomplete/search_provider_unittest.cc',
        'browser/back_forward_menu_model_unittest.cc',
        'browser/blocked_popup_container_unittest.cc',
        'browser/bookmarks/bookmark_codec_unittest.cc',
        'browser/bookmarks/bookmark_drag_data_unittest.cc',
        'browser/bookmarks/bookmark_folder_tree_model_unittest.cc',
        'browser/bookmarks/bookmark_html_writer_unittest.cc',
        'browser/bookmarks/bookmark_index_unittest.cc',
        'browser/bookmarks/bookmark_model_test_utils.cc',
        'browser/bookmarks/bookmark_model_test_utils.h',
        'browser/bookmarks/bookmark_model_unittest.cc',
        'browser/bookmarks/bookmark_table_model_unittest.cc',
        'browser/bookmarks/bookmark_utils_unittest.cc',
        'browser/browser_commands_unittest.cc',
        'browser/browser_theme_provider_unittest.cc',
        'browser/browser_unittest.cc',
        'browser/chrome_browser_application_mac_unittest.mm',
        'browser/debugger/devtools_remote_message_unittest.cc',
        'browser/debugger/devtools_remote_listen_socket_unittest.cc',
        'browser/debugger/devtools_remote_listen_socket_unittest.h',
        'browser/child_process_security_policy_unittest.cc',
        'browser/chrome_thread_unittest.cc',
        'browser/chromeos/external_cookie_handler_unittest.cc',
        'browser/chromeos/external_metrics_unittest.cc',
        'browser/chromeos/gview_request_interceptor_unittest.cc',
        'browser/chromeos/pipe_reader_unittest.cc',
        'browser/chromeos/version_loader_unittest.cc',
        # It is safe to list */cocoa/* files in the "common" file list
        # without an explicit exclusion since gyp is smart enough to
        # exclude them from non-Mac builds.
        'browser/cocoa/about_ipc_controller_unittest.mm',
        'browser/cocoa/about_window_controller_unittest.mm',
        'browser/cocoa/animatable_view_unittest.mm',
        'browser/cocoa/autocomplete_text_field_cell_unittest.mm',
        'browser/cocoa/autocomplete_text_field_editor_unittest.mm',
        'browser/cocoa/autocomplete_text_field_unittest.mm',
        'browser/cocoa/autocomplete_text_field_unittest_helper.mm',
        'browser/cocoa/base_view_unittest.mm',
        'browser/cocoa/background_gradient_view_unittest.mm',
        'browser/cocoa/background_tile_view_unittest.mm',
        'browser/cocoa/blocked_popup_container_controller_unittest.mm',
        'browser/cocoa/bookmark_all_tabs_controller_unittest.mm',
        'browser/cocoa/bookmark_bar_bridge_unittest.mm',
        'browser/cocoa/bookmark_bar_controller_unittest.mm',
        'browser/cocoa/bookmark_bar_toolbar_view_unittest.mm',
        'browser/cocoa/bookmark_bar_view_unittest.mm',
        'browser/cocoa/bookmark_bubble_controller_unittest.mm',
        'browser/cocoa/bookmark_button_unittest.mm',
        'browser/cocoa/bookmark_button_cell_unittest.mm',
        'browser/cocoa/bookmark_editor_base_controller_unittest.mm',
        'browser/cocoa/bookmark_editor_controller_unittest.mm',
        'browser/cocoa/bookmark_menu_unittest.mm',
        'browser/cocoa/bookmark_menu_bridge_unittest.mm',
        'browser/cocoa/bookmark_menu_cocoa_controller_unittest.mm',
        'browser/cocoa/bookmark_name_folder_controller_unittest.mm',
        'browser/cocoa/bookmark_tree_browser_cell_unittest.mm',
        'browser/cocoa/browser_frame_view_unittest.mm',
        'browser/cocoa/browser_window_cocoa_unittest.mm',
        'browser/cocoa/browser_window_controller_unittest.mm',
        'browser/cocoa/bubble_view_unittest.mm',
        'browser/cocoa/bug_report_window_controller_unittest.mm',
        'browser/cocoa/chrome_browser_window_unittest.mm',
        'browser/cocoa/chrome_event_processing_window_unittest.mm',
        'browser/cocoa/clear_browsing_data_controller_unittest.mm',
        'browser/cocoa/clickhold_button_cell_unittest.mm',
        'browser/cocoa/cocoa_test_helper.h',
        'browser/cocoa/cocoa_test_helper.mm',
        'browser/cocoa/command_observer_bridge_unittest.mm',
        'browser/cocoa/custom_home_pages_model_unittest.mm',
        'browser/cocoa/delayedmenu_button_unittest.mm',
        'browser/cocoa/download_shelf_mac_unittest.mm',
        'browser/cocoa/download_shelf_view_unittest.mm',
        'browser/cocoa/download_util_mac_unittest.mm',
        'browser/cocoa/edit_search_engine_cocoa_controller_unittest.mm',
        'browser/cocoa/extension_shelf_controller_unittest.mm',
        'browser/cocoa/event_utils_unittest.mm',
        'browser/cocoa/fast_resize_view_unittest.mm',
        'browser/cocoa/find_bar_bridge_unittest.mm',
        'browser/cocoa/find_bar_cocoa_controller_unittest.mm',
        'browser/cocoa/find_bar_text_field_unittest.mm',
        'browser/cocoa/find_bar_text_field_cell_unittest.mm',
        'browser/cocoa/find_bar_view_unittest.mm',
        'browser/cocoa/find_pasteboard_unittest.mm',
        'browser/cocoa/focus_tracker_unittest.mm',
        'browser/cocoa/fullscreen_window_unittest.mm',
        'browser/cocoa/html_dialog_window_controller_unittest.mm',
        'browser/cocoa/hung_renderer_controller_unittest.mm',
        'browser/cocoa/info_bubble_view_unittest.mm',
        'browser/cocoa/info_bubble_window_unittest.mm',
        'browser/cocoa/infobar_container_controller_unittest.mm',
        'browser/cocoa/infobar_controller_unittest.mm',
        'browser/cocoa/infobar_gradient_view_unittest.mm',
        'browser/cocoa/infobar_text_field_unittest.mm',
        'browser/cocoa/keyword_editor_cocoa_controller_unittest.mm',
        'browser/cocoa/location_bar_view_mac_unittest.mm',
        'browser/cocoa/gradient_button_cell_unittest.mm',
        'browser/cocoa/history_menu_bridge_unittest.mm',
        'browser/cocoa/history_menu_cocoa_controller_unittest.mm',
        'browser/cocoa/hyperlink_button_cell_unittest.mm',
        'browser/cocoa/menu_button_unittest.mm',
        'browser/cocoa/nsimage_cache_unittest.mm',
        'browser/cocoa/nsmenuitem_additions_unittest.mm',
        'browser/cocoa/nswindow_local_state_unittest.mm',
        'browser/cocoa/objc_method_swizzle_unittest.mm',
        'browser/cocoa/page_info_window_controller_unittest.mm',
        'browser/cocoa/preferences_window_controller_unittest.mm',
        'browser/cocoa/rwhvm_editcommand_helper_unittest.mm',
        'browser/cocoa/sad_tab_view_unittest.mm',
        'browser/cocoa/search_engine_list_model_unittest.mm',
        'browser/cocoa/status_bubble_mac_unittest.mm',
        'browser/cocoa/styled_text_field_test_helper.h',
        'browser/cocoa/styled_text_field_test_helper.mm',
        'browser/cocoa/styled_text_field_unittest.mm',
        'browser/cocoa/styled_text_field_cell_unittest.mm',
        'browser/cocoa/tab_controller_unittest.mm',
        'browser/cocoa/tab_strip_controller_unittest.mm',
        'browser/cocoa/tab_strip_view_unittest.mm',
        'browser/cocoa/tab_view_unittest.mm',
        'browser/cocoa/task_manager_mac_unittest.mm',
        'browser/cocoa/test_event_utils.h',
        'browser/cocoa/test_event_utils.mm',
        'browser/cocoa/throbber_view_unittest.mm',
        'browser/cocoa/toolbar_button_cell_unittest.mm',
        'browser/cocoa/toolbar_controller_unittest.mm',
        'browser/cocoa/toolbar_view_unittest.mm',
        'browser/cocoa/view_resizer_pong.h',
        'browser/cocoa/view_resizer_pong.mm',
        'browser/cocoa/web_drop_target_unittest.mm',
        'browser/command_updater_unittest.cc',
        'browser/cookies_tree_model_unittest.cc',
        'browser/debugger/devtools_manager_unittest.cc',
        'browser/dom_ui/dom_ui_theme_source_unittest.cc',
        'browser/dom_ui/dom_ui_unittest.cc',
        'browser/dom_ui/shown_sections_handler_unittest.cc',
        'browser/download/download_manager_unittest.cc',
        'browser/download/download_request_infobar_delegate_unittest.cc',
        'browser/download/download_request_manager_unittest.cc',
        'browser/download/save_package_unittest.cc',
        'browser/encoding_menu_controller_unittest.cc',
        'browser/extensions/convert_user_script_unittest.cc',
        'browser/extensions/extension_file_util_unittest.cc',
        'browser/extensions/extension_messages_unittest.cc',
        'browser/extensions/extension_process_manager_unittest.cc',
        'browser/extensions/extension_ui_unittest.cc',
        'browser/extensions/extension_updater_unittest.cc',
        'browser/extensions/extensions_service_unittest.cc',
        'browser/extensions/file_reader_unittest.cc',
        'browser/extensions/sandboxed_extension_unpacker_unittest.cc',
        'browser/extensions/user_script_listener_unittest.cc',
        'browser/extensions/user_script_master_unittest.cc',
        'browser/find_backend_unittest.cc',
        'browser/global_keyboard_shortcuts_mac_unittest.cc',
        'browser/google_url_tracker_unittest.cc',
        'browser/google_update_settings_posix_unittest.cc',
        'browser/gtk/bookmark_bar_gtk_unittest.cc',
        'browser/gtk/bookmark_editor_gtk_unittest.cc',
        'browser/gtk/gtk_theme_provider_unittest.cc',
        'browser/gtk/go_button_gtk_unittest.cc',
        'browser/gtk/keyword_editor_view_unittest.cc',
        'browser/gtk/options/cookies_view_unittest.cc',
        'browser/gtk/options/languages_page_gtk_unittest.cc',
        'browser/gtk/tabs/tab_renderer_gtk_unittest.cc',
        'browser/history/expire_history_backend_unittest.cc',
        'browser/history/history_backend_unittest.cc',
        'browser/history/history_querying_unittest.cc',
        'browser/history/history_types_unittest.cc',
        'browser/history/history_unittest.cc',
        'browser/history/query_parser_unittest.cc',
        'browser/history/snippet_unittest.cc',
        'browser/history/starred_url_database_unittest.cc',
        'browser/history/text_database_manager_unittest.cc',
        'browser/history/text_database_unittest.cc',
        'browser/history/thumbnail_database_unittest.cc',
        'browser/history/top_sites_unittest.cc',
        'browser/thumbnail_store_unittest.cc',
        'browser/history/url_database_unittest.cc',
        'browser/history/visit_database_unittest.cc',
        'browser/history/visit_tracker_unittest.cc',
        'browser/importer/firefox_importer_unittest_messages_internal.h',
        'browser/importer/firefox_importer_unittest.cc',
        'browser/importer/firefox_importer_unittest_utils.h',
        'browser/importer/firefox_importer_unittest_utils_mac.cc',
        'browser/importer/importer_unittest.cc',
        'browser/importer/safari_importer_unittest.mm',
        'browser/importer/toolbar_importer_unittest.cc',
        'browser/importer/firefox_profile_lock_unittest.cc',
        'browser/in_process_webkit/dom_storage_dispatcher_host_unittest.cc',
        'browser/in_process_webkit/webkit_context_unittest.cc',
        'browser/in_process_webkit/webkit_thread_unittest.cc',
        'browser/keychain_mock_mac.cc',
        'browser/keychain_mock_mac.h',
        'browser/login_prompt_unittest.cc',
        'browser/meta_table_helper_unittest.cc',
        'browser/metrics/metrics_log_unittest.cc',
        'browser/metrics/metrics_response_unittest.cc',
        'browser/metrics/metrics_service_unittest.cc',
        'browser/net/chrome_url_request_context_unittest.cc',
        'browser/net/dns_host_info_unittest.cc',
        'browser/net/dns_master_unittest.cc',
        'browser/net/resolve_proxy_msg_helper_unittest.cc',
        'browser/net/test_url_fetcher_factory.cc',
        'browser/net/test_url_fetcher_factory.h',
        'browser/net/url_fetcher_unittest.cc',
        'browser/net/url_fixer_upper_unittest.cc',
        'browser/notifications/desktop_notifications_unittest.cc',
        'browser/notifications/desktop_notifications_unittest.h',
        'browser/password_manager/encryptor_unittest.cc',
        'browser/password_manager/login_database_unittest.cc',
        'browser/password_manager/password_form_manager_unittest.cc',
        'browser/password_manager/password_store_mac_unittest.cc',
        'browser/printing/print_job_unittest.cc',
        'browser/privacy_blacklist/blacklist_manager_unittest.cc',
        'browser/privacy_blacklist/blacklist_unittest.cc',
        'browser/privacy_blacklist/blacklist_io_unittest.cc',
        'browser/process_info_snapshot_mac_unittest.cc',
        'browser/profile_manager_unittest.cc',
        'browser/renderer_host/audio_renderer_host_unittest.cc',
        'browser/renderer_host/render_widget_host_unittest.cc',
        'browser/renderer_host/resource_dispatcher_host_unittest.cc',
        'browser/renderer_host/test/render_view_host_unittest.cc',
        'browser/renderer_host/test/site_instance_unittest.cc',
        'browser/renderer_host/web_cache_manager_unittest.cc',
        'browser/rlz/rlz_unittest.cc',
        'browser/safe_browsing/bloom_filter_unittest.cc',
        'browser/safe_browsing/chunk_range_unittest.cc',
        'browser/safe_browsing/protocol_manager_unittest.cc',
        'browser/safe_browsing/protocol_parser_unittest.cc',
        'browser/safe_browsing/safe_browsing_blocking_page_unittest.cc',
        'browser/safe_browsing/safe_browsing_database_unittest.cc',
        'browser/safe_browsing/safe_browsing_util_unittest.cc',
        'browser/search_engines/keyword_editor_controller_unittest.cc',
        'browser/search_engines/template_url_model_unittest.cc',
        'browser/search_engines/template_url_parser_unittest.cc',
        'browser/search_engines/template_url_prepopulate_data_unittest.cc',
        'browser/search_engines/template_url_scraper_unittest.cc',
        'browser/search_engines/template_url_unittest.cc',
        'browser/sessions/session_backend_unittest.cc',
        'browser/sessions/session_service_test_helper.cc',
        'browser/sessions/session_service_test_helper.h',
        'browser/sessions/session_service_unittest.cc',
        'browser/sessions/tab_restore_service_unittest.cc',
        'browser/shell_integration_unittest.cc',
        'browser/spellcheck_unittest.cc',
        'browser/ssl/ssl_host_state_unittest.cc',
        'browser/sync/glue/bookmark_model_worker_unittest.cc',
        'browser/sync/glue/http_bridge_unittest.cc',
        'browser/sync/profile_sync_service_unittest.cc',
        'browser/sync/sync_setup_wizard_unittest.cc',
        'browser/tab_contents/navigation_controller_unittest.cc',
        'browser/tab_contents/navigation_entry_unittest.cc',
        'browser/tab_contents/render_view_host_manager_unittest.cc',
        'browser/tab_contents/thumbnail_generator_unittest.cc',
        'browser/tab_contents/web_contents_unittest.cc',
        'browser/tabs/tab_strip_model_unittest.cc',
        'browser/task_manager_unittest.cc',
        'browser/theme_resources_util_unittest.cc',
        'browser/views/bookmark_context_menu_test.cc',
        'browser/views/bookmark_editor_view_unittest.cc',
        'browser/visitedlink_unittest.cc',
        'browser/webdata/web_database_unittest.cc',
        'browser/window_sizer_unittest.cc',
        'common/bzip2_unittest.cc',
        'common/child_process_logging_mac_unittest.mm',
        'common/chrome_plugin_unittest.cc',
        'common/common_param_traits_unittest.cc',
        'common/extensions/extension_resource_unittest.cc',
        'common/extensions/extension_unittest.cc',
        'common/extensions/extension_action_unittest.cc',
        'common/extensions/extension_l10n_util_unittest.cc',
        'common/extensions/extension_message_bundle_unittest.cc',
        'common/extensions/extension_unpacker_unittest.cc',
        'common/extensions/update_manifest_unittest.cc',
        'common/extensions/url_pattern_unittest.cc',
        'common/extensions/user_script_unittest.cc',
        'common/important_file_writer_unittest.cc',
        'common/json_value_serializer_unittest.cc',
        'common/mru_cache_unittest.cc',
        'common/net/url_util_unittest.cc',
        'common/notification_service_unittest.cc',
        'common/pref_member_unittest.cc',
        'common/pref_service_unittest.cc',
        'common/property_bag_unittest.cc',
        'common/resource_dispatcher_unittest.cc',
        'common/thumbnail_score_unittest.cc',
        'common/time_format_unittest.cc',
        'common/worker_thread_ticker_unittest.cc',
        'common/zip_unittest.cc',
        'renderer/audio_message_filter_unittest.cc',
        'renderer/media/audio_renderer_impl_unittest.cc',
        'renderer/extensions/extension_api_client_unittest.cc',
        'renderer/extensions/json_schema_unittest.cc',
        'renderer/net/render_dns_master_unittest.cc',
        'renderer/net/render_dns_queue_unittest.cc',
        'renderer/render_process_unittest.cc',
        'renderer/render_thread_unittest.cc',
        'renderer/render_view_unittest.cc',
        'renderer/render_view_unittest_mac.mm',
        'renderer/render_widget_unittest.cc',
        'renderer/renderer_main_unittest.cc',
        'test/browser_with_test_window_test.cc',
        'test/browser_with_test_window_test.h',
        'test/file_test_utils.cc',
        'test/file_test_utils.h',
        'test/render_view_test.cc',
        'test/render_view_test.h',
        'test/sync/test_http_bridge_factory.h',
        'test/test_notification_tracker.cc',
        'test/test_notification_tracker.h',
        'test/v8_unit_test.cc',
        'test/v8_unit_test.h',
        '../third_party/cld/bar/toolbar/cld/i18n/encodings/compact_lang_det/compact_lang_det_unittest_small.cc',
      ],
      'conditions': [
        ['chromeos==0 and toolkit_views==0', {
          'sources/': [
            ['exclude', '^browser/chromeos'],
          ],
        }],
        ['OS=="linux" and selinux==0', {
          'dependencies': [
            '../sandbox/sandbox.gyp:*',
          ],
        }],
        ['OS=="linux"', {
          'conditions': [
            [ 'gcc_version==44', {
              # Avoid gcc 4.4 strict aliasing issues in stl_tree.h when
              # building mru_cache_unittest.cc.
              'cflags': [
                '-fno-strict-aliasing',
              ],
            }],
          ],
          'dependencies': [
            '../build/linux/system.gyp:gtk',
            '../build/linux/system.gyp:nss',
            '../tools/xdisplaycheck/xdisplaycheck.gyp:xdisplaycheck',
          ],
          'sources': [
            'browser/renderer_host/gtk_key_bindings_handler_unittest.cc',
          ],
          'sources!': [
            'browser/views/bookmark_context_menu_test.cc',
            'browser/gtk/options/cookies_view_unittest.cc',
            # Compact Language Detection (cld) is not supported in linux yet.
            '../third_party/cld/bar/toolbar/cld/i18n/encodings/compact_lang_det/compact_lang_det_unittest_small.cc',
          ],
        }],
        ['OS=="linux" and (toolkit_views==1 or chromeos==1)', {
          'dependencies': [
            '../views/views.gyp:views',
          ],
          'sources': [
            '<@(views_unit_tests_sources)',
          ],
          # We must use 'sources/' instead of 'source!' as there is a
          # target-default 'sources/' including gtk_unittest and 'source/' takes
          # precedence over 'sources!'.
          'sources/': [
             ['exclude', 'browser/gtk/bookmark_bar_gtk_unittest\\.cc$'],
             ['exclude', 'browser/gtk/bookmark_editor_gtk_unittest\\.cc$'],
             ['exclude', 'browser/gtk/go_button_gtk_unittest\\.cc$'],
             ['exclude', 'browser/gtk/tabs/tab_renderer_gtk_unittest\\.cc$'],
             ['exclude', 'browser/gtk/options/cookies_view_unittest\\.cc$'],
             ['exclude', 'browser/gtk/options/languages_page_gtk_unittest\\.cc$'],
             ['exclude', 'browser/gtk/gtk_theme_provider_unittest\\.cc$'],
          ],
          'conditions': [
            ['chromeos==0', {
              'sources/': [
                 ['include', 'browser/views/bookmark_context_menu_test.cc$'],
              ],
            }]
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
            'browser/views/bookmark_context_menu_test.cc',
            'browser/back_forward_menu_model_unittest.cc',
            'browser/gtk/go_button_gtk_unittest.cc',
            'browser/gtk/tabs/tab_renderer_gtk_unittest.cc',
            'browser/tab_contents/navigation_controller_unittest.cc',
            'browser/task_manager_unittest.cc',
            '../third_party/hunspell/google/hunspell_tests.cc',

            # Compact Language Detection (cld) is not supported in mac yet.
            '../third_party/cld/bar/toolbar/cld/i18n/encodings/compact_lang_det/compact_lang_det_unittest_small.cc',
          ],
          # TODO(mark): We really want this for all non-static library targets,
          # but when we tried to pull it up to the common.gypi level, it broke
          # other things like the ui, startup, and page_cycler tests. *shrug*
          'xcode_settings': {'OTHER_LDFLAGS': ['-Wl,-ObjC']},
        }, { # OS != "mac"
          'dependencies': [
            'convert_dict_lib',
            '../third_party/hunspell/hunspell.gyp:hunspell',
          ],
        }],
        ['OS=="win"', {
          'defines': [
            '__STD_C',
            '_CRT_SECURE_NO_DEPRECATE',
            '_SCL_SECURE_NO_DEPRECATE',
          ],
          'dependencies': [
            'chrome_dll_version',
            'installer/installer.gyp:installer_util_strings',
            '../views/views.gyp:views',
            'test_chrome_plugin',  # run time dependency
            '../third_party/tcmalloc/tcmalloc.gyp:tcmalloc',
          ],
          'include_dirs': [
            'third_party/wtl/include',
          ],
          'sources': [
            'app/chrome_dll.rc',
            'test/data/resource.rc',

            '<@(views_unit_tests_sources)',
            '<@(views_unit_tests_sources_win_specific)',

            # TODO:  It would be nice to have these pulled in
            # automatically from direct_dependent_settings in
            # their various targets (net.gyp:net_resources, etc.),
            # but that causes errors in other targets when
            # resulting .res files get referenced multiple times.
            '<(SHARED_INTERMEDIATE_DIR)/app/app_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/browser_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/common_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/renderer_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/theme_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/net/net_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/webkit_resources.rc',
          ],
          'sources!': [
            'browser/gtk/tabs/tab_renderer_gtk_unittest.cc',
            'common/net/url_util_unittest.cc',
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
            'Debug': {
              'msvs_settings': {
                'VCLinkerTool': {
                  'LinkIncremental': '<(msvs_large_module_debug_link_mode)',
                },
              },
            },
          },
        }, { # else: OS != "win"
          'sources!': [
            'browser/bookmarks/bookmark_codec_unittest.cc',
            'browser/bookmarks/bookmark_drag_data_unittest.cc',
            'browser/bookmarks/bookmark_folder_tree_model_unittest.cc',
            'browser/bookmarks/bookmark_table_model_unittest.cc',
            'browser/browser_unittest.cc',
            'browser/extensions/extension_process_manager_unittest.cc',
            'browser/importer/importer_unittest.cc',
            'browser/login_prompt_unittest.cc',
            'browser/printing/print_job_unittest.cc',
            'browser/rlz/rlz_unittest.cc',
            'browser/safe_browsing/safe_browsing_blocking_page_unittest.cc',
            'browser/search_engines/template_url_scraper_unittest.cc',
            'browser/views/bookmark_editor_view_unittest.cc',
            'browser/views/find_bar_host_unittest.cc',
            'browser/views/keyword_editor_view_unittest.cc',
            'common/chrome_plugin_unittest.cc',
            'common/net/url_util_unittest.cc',
          ],
        }],
      ],
    },
    {
      'target_name': 'startup_tests',
      'type': 'executable',
      'msvs_guid': 'D3E6C0FD-54C7-4FF2-9AE1-72F2DAFD820C',
      'dependencies': [
        'chrome',
        'browser',
        'debugger',
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
        'test/startup/startup_test.cc',
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
        ['OS=="win"', {
          'dependencies': [
            '../third_party/tcmalloc/tcmalloc.gyp:tcmalloc',
          ],
        },],
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
        ['OS=="win"', {
          'dependencies': [
            '../third_party/tcmalloc/tcmalloc.gyp:tcmalloc',
          ],
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
        ['OS=="linux"', {
          'dependencies': [
            '../build/linux/system.gyp:gtk',
            '../tools/xdisplaycheck/xdisplaycheck.gyp:xdisplaycheck',
          ],
        }],
        ['OS=="win" or (OS=="linux" and toolkit_views==1)', {
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
        'action': ['$(TargetPath)', '--gtest_print_time', '-enable-logging',
                   '-dump-histograms-on-exit', '-log-level=0'],
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
        ['OS=="win"', {
          'dependencies': [
            '../third_party/tcmalloc/tcmalloc.gyp:tcmalloc',
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
            'third_party/wtl/include',
          ],
          'dependencies': [
            '../third_party/tcmalloc/tcmalloc.gyp:tcmalloc',
          ],
        }], # OS="win"
      ], # conditions
    },
    {
      'target_name': 'sync_unit_tests',
      'type': 'executable',
      'sources': [
        'app/breakpad_mac_stubs.mm',
        'app/keystone_glue.mm',
        'browser/sync/engine/all_status_unittest.cc',
        'browser/sync/engine/apply_updates_command_unittest.cc',
        'browser/sync/engine/auth_watcher_unittest.cc',
        'browser/sync/engine/net/gaia_authenticator_unittest.cc',
        'browser/sync/engine/syncer_proto_util_unittest.cc',
        'browser/sync/engine/syncer_thread_unittest.cc',
        'browser/sync/engine/syncer_unittest.cc',
        'browser/sync/engine/syncproto_unittest.cc',
        'browser/sync/notifier/listener/talk_mediator_unittest.cc',
        'browser/sync/syncable/syncable_id_unittest.cc',
        'browser/sync/syncable/syncable_unittest.cc',
        'browser/sync/util/character_set_converters_unittest.cc',
        'browser/sync/util/crypto_helpers_unittest.cc',
        'browser/sync/util/event_sys_unittest.cc',
        'browser/sync/util/extensions_activity_monitor_unittest.cc',
        'browser/sync/util/path_helpers_unittest.cc',
        'browser/sync/util/query_helpers_unittest.cc',
        'browser/sync/util/user_settings_unittest.cc',
        'test/file_test_utils.cc',
        'test/sync/engine/mock_gaia_authenticator.cc',
        'test/sync/engine/mock_gaia_authenticator.h',
        'test/sync/engine/mock_gaia_authenticator_unittest.cc',
        'test/sync/engine/mock_server_connection.cc',
        'test/sync/engine/mock_server_connection.h',
        'test/sync/engine/test_directory_setter_upper.cc',
        'test/sync/engine/test_directory_setter_upper.h',
        'test/sync/engine/test_id_factory.h',
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
       'common',
       'debugger',
       '../skia/skia.gyp:skia',
       '../testing/gtest.gyp:gtest',
       '../third_party/libjingle/libjingle.gyp:libjingle',
       'syncapi',
       'sync_proto',
       'test_support_unit',
      ],
      'conditions': [
        ['OS=="win"', {
          'sources' : [
            'browser/sync/util/data_encryption_unittest.cc',
          ],
          'dependencies': [
            '../third_party/tcmalloc/tcmalloc.gyp:tcmalloc',
          ],
          'link_settings': {
            'libraries': [
              '-lcrypt32.lib',
              '-lws2_32.lib',
              '-lsecur32.lib',
            ],
          },
        }, { # else: OS != "win"
          'sources!': [
            'browser/sync/util/data_encryption_unittest.cc',
          ],
        }],
        ['OS=="linux"', {
          'dependencies': [
            '../build/linux/system.gyp:gtk'
          ],
        }],
      ],
    },
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
