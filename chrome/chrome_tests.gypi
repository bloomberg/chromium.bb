# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'variables': {
    'build_angle_deqp_tests%': 0,
    # TODO(rockot) bug 505926: These should be moved to extensions_browsertests
    # but have old dependencies on chrome files. The chrome dependencies should
    # be removed and these moved to the extensions_browsertests target.
    'chrome_browser_tests_extensions_sources': [
      '../extensions/browser/api/app_window/app_window_apitest.cc',
      '../extensions/browser/api/bluetooth/bluetooth_apitest.cc',
      '../extensions/browser/api/bluetooth/bluetooth_private_apitest.cc',
      '../extensions/browser/api/cast_channel/cast_channel_apitest.cc',
      '../extensions/browser/api/runtime/runtime_apitest.cc',
      '../extensions/browser/api/serial/serial_apitest.cc',
      '../extensions/browser/api/usb/usb_manual_apitest.cc',
      '../extensions/browser/app_window/app_window_browsertest.cc',
      '../extensions/browser/guest_view/extension_options/extension_options_apitest.cc',
      '../extensions/browser/guest_view/mime_handler_view/mime_handler_view_browsertest.cc',
      '../extensions/renderer/console_apitest.cc',
      '../extensions/renderer/script_context_browsertest.cc'
    ],
    'chrome_browser_tests_display_source_apitest': [
    ],
    # Tests corresponding to the files in chrome_browser_ui_cocoa_sources.
    # Built on Mac, except when mac_views_browser==1.
    'chrome_browser_tests_cocoa_sources': [
    ],
    'chrome_browser_tests_views_non_cros_sources': [
      # This should be brought up on OSX Views but not CrOS.
    ],
    'chrome_browser_tests_media_router_sources': [
      'browser/ui/webui/media_router/media_router_dialog_controller_impl_browsertest.cc',
      'test/data/webui/media_router/media_router_elements_browsertest.js',
      'test/media_router/media_router_base_browsertest.cc',
      'test/media_router/media_router_base_browsertest.h',
      'test/media_router/media_router_e2e_browsertest.cc',
      'test/media_router/media_router_e2e_browsertest.h',
      'test/media_router/media_router_e2e_ui_browsertest.cc',
      'test/media_router/media_router_integration_browsertest.cc',
      'test/media_router/media_router_integration_browsertest.h',
      'test/media_router/media_router_integration_ui_browsertest.cc',
      'test/media_router/test_media_sinks_observer.cc',
      'test/media_router/test_media_sinks_observer.h',
    ],
    # TODO(rockot) bug 505926: These should be moved to a target in
    # //extensions but have old dependencies on chrome files. The chrome
    # dependencies should be removed and these moved to the
    # extensions_browsertests target.
    'chrome_interactive_ui_test_extensions_sources': [
      '../extensions/browser/app_window/app_window_interactive_uitest.cc',
    ],
    'chrome_automation_client_lib_sources': [
      'test/chromedriver/chrome/adb.h',
      'test/chromedriver/chrome/adb_impl.cc',
      'test/chromedriver/chrome/adb_impl.h',
      'test/chromedriver/chrome/automation_extension.cc',
      'test/chromedriver/chrome/automation_extension.h',
      'test/chromedriver/chrome/chrome.h',
      'test/chromedriver/chrome/chrome_android_impl.cc',
      'test/chromedriver/chrome/chrome_android_impl.h',
      'test/chromedriver/chrome/chrome_desktop_impl.cc',
      'test/chromedriver/chrome/chrome_desktop_impl.h',
      'test/chromedriver/chrome/chrome_finder.cc',
      'test/chromedriver/chrome/chrome_finder.h',
      'test/chromedriver/chrome/chrome_finder_mac.mm',
      'test/chromedriver/chrome/chrome_impl.cc',
      'test/chromedriver/chrome/chrome_impl.h',
      'test/chromedriver/chrome/chrome_remote_impl.cc',
      'test/chromedriver/chrome/chrome_remote_impl.h',
      'test/chromedriver/chrome/console_logger.cc',
      'test/chromedriver/chrome/console_logger.h',
      'test/chromedriver/chrome/debugger_tracker.cc',
      'test/chromedriver/chrome/debugger_tracker.h',
      'test/chromedriver/chrome/device_manager.cc',
      'test/chromedriver/chrome/device_manager.h',
      'test/chromedriver/chrome/device_metrics.cc',
      'test/chromedriver/chrome/device_metrics.h',
      'test/chromedriver/chrome/devtools_client.h',
      'test/chromedriver/chrome/devtools_client_impl.cc',
      'test/chromedriver/chrome/devtools_client_impl.h',
      'test/chromedriver/chrome/devtools_event_listener.cc',
      'test/chromedriver/chrome/devtools_event_listener.h',
      'test/chromedriver/chrome/devtools_http_client.cc',
      'test/chromedriver/chrome/devtools_http_client.h',
      'test/chromedriver/chrome/dom_tracker.cc',
      'test/chromedriver/chrome/dom_tracker.h',
      'test/chromedriver/chrome/frame_tracker.cc',
      'test/chromedriver/chrome/frame_tracker.h',
      'test/chromedriver/chrome/geolocation_override_manager.cc',
      'test/chromedriver/chrome/geolocation_override_manager.h',
      'test/chromedriver/chrome/geoposition.h',
      'test/chromedriver/chrome/heap_snapshot_taker.cc',
      'test/chromedriver/chrome/heap_snapshot_taker.h',
      'test/chromedriver/chrome/javascript_dialog_manager.cc',
      'test/chromedriver/chrome/javascript_dialog_manager.h',
      'test/chromedriver/chrome/log.cc',
      'test/chromedriver/chrome/log.h',
      'test/chromedriver/chrome/mobile_device.cc',
      'test/chromedriver/chrome/mobile_device.h',
      'test/chromedriver/chrome/mobile_device_list.cc',
      'test/chromedriver/chrome/mobile_device_list.h',
      'test/chromedriver/chrome/mobile_emulation_override_manager.cc',
      'test/chromedriver/chrome/mobile_emulation_override_manager.h',
      'test/chromedriver/chrome/navigation_tracker.cc',
      'test/chromedriver/chrome/navigation_tracker.h',
      'test/chromedriver/chrome/network_conditions.cc',
      'test/chromedriver/chrome/network_conditions.h',
      'test/chromedriver/chrome/network_conditions_override_manager.cc',
      'test/chromedriver/chrome/network_conditions_override_manager.h',
      'test/chromedriver/chrome/network_list.cc',
      'test/chromedriver/chrome/network_list.h',
      'test/chromedriver/chrome/non_blocking_navigation_tracker.cc',
      'test/chromedriver/chrome/non_blocking_navigation_tracker.h',
      'test/chromedriver/chrome/page_load_strategy.cc',
      'test/chromedriver/chrome/page_load_strategy.h',
      'test/chromedriver/chrome/status.cc',
      'test/chromedriver/chrome/status.h',
      'test/chromedriver/chrome/ui_events.cc',
      'test/chromedriver/chrome/ui_events.h',
      'test/chromedriver/chrome/util.cc',
      'test/chromedriver/chrome/util.h',
      'test/chromedriver/chrome/version.cc',
      'test/chromedriver/chrome/version.h',
      'test/chromedriver/chrome/web_view.h',
      'test/chromedriver/chrome/web_view_impl.cc',
      'test/chromedriver/chrome/web_view_impl.h',
      'test/chromedriver/net/adb_client_socket.cc',
      'test/chromedriver/net/adb_client_socket.h',
      'test/chromedriver/net/net_util.cc',
      'test/chromedriver/net/net_util.h',
      'test/chromedriver/net/port_server.cc',
      'test/chromedriver/net/port_server.h',
      'test/chromedriver/net/sync_websocket.h',
      'test/chromedriver/net/sync_websocket_factory.cc',
      'test/chromedriver/net/sync_websocket_factory.h',
      'test/chromedriver/net/sync_websocket_impl.cc',
      'test/chromedriver/net/sync_websocket_impl.h',
      'test/chromedriver/net/timeout.cc',
      'test/chromedriver/net/timeout.h',
      'test/chromedriver/net/url_request_context_getter.cc',
      'test/chromedriver/net/url_request_context_getter.h',
      'test/chromedriver/net/websocket.cc',
      'test/chromedriver/net/websocket.h',
    ],
    'chrome_driver_lib_sources': [
      '../third_party/webdriver/atoms.cc',
      '../third_party/webdriver/atoms.h',
      'common/chrome_constants.cc',
      'common/chrome_constants.h',
      'test/chromedriver/alert_commands.cc',
      'test/chromedriver/alert_commands.h',
      'test/chromedriver/basic_types.cc',
      'test/chromedriver/basic_types.h',
      'test/chromedriver/capabilities.cc',
      'test/chromedriver/capabilities.h',
      'test/chromedriver/chrome/browser_info.cc',
      'test/chromedriver/chrome/browser_info.h',
      'test/chromedriver/chrome_launcher.cc',
      'test/chromedriver/chrome_launcher.h',
      'test/chromedriver/command.h',
      'test/chromedriver/command_listener.h',
      'test/chromedriver/command_listener_proxy.cc',
      'test/chromedriver/command_listener_proxy.h',
      'test/chromedriver/commands.cc',
      'test/chromedriver/commands.h',
      'test/chromedriver/element_commands.cc',
      'test/chromedriver/element_commands.h',
      'test/chromedriver/element_util.cc',
      'test/chromedriver/element_util.h',
      'test/chromedriver/key_converter.cc',
      'test/chromedriver/key_converter.h',
      'test/chromedriver/keycode_text_conversion.h',
      'test/chromedriver/keycode_text_conversion_mac.mm',
      'test/chromedriver/keycode_text_conversion_ozone.cc',
      'test/chromedriver/keycode_text_conversion_win.cc',
      'test/chromedriver/keycode_text_conversion_x.cc',
      'test/chromedriver/logging.cc',
      'test/chromedriver/logging.h',
      'test/chromedriver/performance_logger.cc',
      'test/chromedriver/performance_logger.h',
      'test/chromedriver/server/http_handler.cc',
      'test/chromedriver/server/http_handler.h',
      'test/chromedriver/session.cc',
      'test/chromedriver/session.h',
      'test/chromedriver/session_commands.cc',
      'test/chromedriver/session_commands.h',
      'test/chromedriver/session_thread_map.h',
      'test/chromedriver/util.cc',
      'test/chromedriver/util.h',
      'test/chromedriver/window_commands.cc',
      'test/chromedriver/window_commands.h',
    ],
    'chrome_driver_unittests_sources': [
      'test/chromedriver/capabilities_unittest.cc',
      'test/chromedriver/chrome/browser_info_unittest.cc',
      'test/chromedriver/chrome/chrome_finder_unittest.cc',
      'test/chromedriver/chrome/console_logger_unittest.cc',
      'test/chromedriver/chrome/device_manager_unittest.cc',
      'test/chromedriver/chrome/devtools_client_impl_unittest.cc',
      'test/chromedriver/chrome/devtools_http_client_unittest.cc',
      'test/chromedriver/chrome/dom_tracker_unittest.cc',
      'test/chromedriver/chrome/frame_tracker_unittest.cc',
      'test/chromedriver/chrome/geolocation_override_manager_unittest.cc',
      'test/chromedriver/chrome/heap_snapshot_taker_unittest.cc',
      'test/chromedriver/chrome/javascript_dialog_manager_unittest.cc',
      'test/chromedriver/chrome/mobile_emulation_override_manager_unittest.cc',
      'test/chromedriver/chrome/navigation_tracker_unittest.cc',
      'test/chromedriver/chrome/network_conditions_override_manager_unittest.cc',
      'test/chromedriver/chrome/recorder_devtools_client.cc',
      'test/chromedriver/chrome/recorder_devtools_client.h',
      'test/chromedriver/chrome/status_unittest.cc',
      'test/chromedriver/chrome/stub_chrome.cc',
      'test/chromedriver/chrome/stub_chrome.h',
      'test/chromedriver/chrome/stub_devtools_client.cc',
      'test/chromedriver/chrome/stub_devtools_client.h',
      'test/chromedriver/chrome/stub_web_view.cc',
      'test/chromedriver/chrome/stub_web_view.h',
      'test/chromedriver/chrome/web_view_impl_unittest.cc',
      'test/chromedriver/chrome_launcher_unittest.cc',
      'test/chromedriver/command_listener_proxy_unittest.cc',
      'test/chromedriver/commands_unittest.cc',
      'test/chromedriver/logging_unittest.cc',
      'test/chromedriver/net/timeout_unittest.cc',
      'test/chromedriver/performance_logger_unittest.cc',
      'test/chromedriver/server/http_handler_unittest.cc',
      'test/chromedriver/session_commands_unittest.cc',
      'test/chromedriver/session_unittest.cc',
      'test/chromedriver/util_unittest.cc',
    ],
    'chrome_driver_tests_sources': [
      'test/chromedriver/key_converter_unittest.cc',
      'test/chromedriver/keycode_text_conversion_unittest.cc',
      'test/chromedriver/net/net_util_unittest.cc',
      'test/chromedriver/net/port_server_unittest.cc',
      'test/chromedriver/net/sync_websocket_impl_unittest.cc',
      'test/chromedriver/net/test_http_server.cc',
      'test/chromedriver/net/test_http_server.h',
      'test/chromedriver/net/websocket_unittest.cc',
      'test/chromedriver/test_util.cc',
      'test/chromedriver/test_util.h',
    ],
  },
  'includes': [
    'js_unittest_vars.gypi',
  ],
  'targets': [
    {
      # This target contains non-unittest test utilities that don't belong in
      # production libraries but are used by more than one test executable.
      #
      # GN version: //chrome/test:test_support_ui
      'target_name': 'test_support_ui',
      'type': 'static_library',
      'dependencies': [
        '../components/components.gyp:metrics_test_support',
        '../components/components.gyp:os_crypt_test_support',
        '../components/components.gyp:password_manager_core_browser_test_support',
        '../skia/skia.gyp:skia',
        '../testing/gtest.gyp:gtest',
        'common',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'browser/password_manager/password_manager_test_base.cc',
        'browser/password_manager/password_manager_test_base.h',
        'browser/signin/token_revoker_test_utils.cc',
        'browser/signin/token_revoker_test_utils.h',
        'browser/ui/webui/signin/login_ui_test_utils.cc',
        'browser/ui/webui/signin/login_ui_test_utils.h',
        'test/base/in_process_browser_test.cc',
        'test/base/in_process_browser_test.h',
        'test/base/in_process_browser_test_mac.cc',
        'test/base/ui_test_utils.cc',
        'test/base/ui_test_utils.h',
      ],
      'conditions': [
        ['enable_plugins==1', {
          "sources" : [
            'test/ppapi/ppapi_test.cc',
            'test/ppapi/ppapi_test.h',
          ],
        }],
      ],
    },
    {
      # GN version: //chrome/test:interactive_ui_tests
      'target_name': 'interactive_ui_tests',
      'type': 'executable',
      'dependencies': [
        'browser',
        'chrome_features.gyp:chrome_common_features',
        'chrome_resources.gyp:chrome_resources',
        'chrome_resources.gyp:chrome_strings',
        'chrome_resources.gyp:packed_extra_resources',
        'chrome_resources.gyp:packed_resources',
        'debugger',
        'renderer',
        'test_support_common',
        'test_support_ui',
        '../components/components.gyp:guest_view_test_support',
        '../components/components_resources.gyp:components_resources',
        '../content/app/resources/content_resources.gyp:content_resources',
        '../content/content_shell_and_tests.gyp:content_browser_test_base',
        '../crypto/crypto.gyp:crypto_test_support',
        '../google_apis/google_apis.gyp:google_apis_test_support',
        '../net/net.gyp:net',
        '../net/net.gyp:net_resources',
        '../net/net.gyp:net_test_support',
        '../ppapi/ppapi_internal.gyp:ppapi_tests',
        '../skia/skia.gyp:skia',
        '../components/sync.gyp:sync',
	'../components/sync.gyp:test_support_sync_api',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        '../third_party/hunspell/hunspell.gyp:hunspell',
        '../third_party/icu/icu.gyp:icui18n',
        '../third_party/icu/icu.gyp:icuuc',
        '../third_party/libpng/libpng.gyp:libpng',
        '../third_party/zlib/zlib.gyp:zlib',
        '../ui/base/ui_base.gyp:ui_base_test_support',
        '../ui/resources/ui_resources.gyp:ui_test_pak',
        '../ui/web_dialogs/web_dialogs.gyp:web_dialogs_test_support',
        # Runtime dependencies
        '../third_party/mesa/mesa.gyp:osmesa',
      ],
      'include_dirs': [
        '..',
      ],
      'defines': [
        'HAS_OUT_OF_PROC_TEST_RUNNER',
      ],
      'sources': [
        '<@(chrome_interactive_ui_test_extensions_sources)',
        '<@(chrome_interactive_ui_test_sources)',
      ],
      'conditions': [
        ['use_x11==1', {
          'dependencies': [
            '../build/linux/system.gyp:xtst',
            '../tools/xdisplaycheck/xdisplaycheck.gyp:xdisplaycheck',
          ],
        }],
        ['OS=="linux"', {
          'dependencies': [
            '../build/linux/system.gyp:nss',
          ],
        }, {  # Non-Linux platforms (Linux includes ChromeOS here).
          'sources': [
            '<@(chrome_interactive_ui_test_non_linux_and_chromeos_sources)',
          ],
        }],
        ['OS=="linux" and chromeos==0', {
          'sources!': [
            # TODO(port): This times out. Attempts have been made to fix the
            # individual failures, but each time I disable a test from these
            # suites, it seems like one or another starts timing out too.

            # Note: list duplicated in GN build.
            'browser/ui/views/keyboard_access_browsertest.cc',
          ],
        }, {  # Everything but desktop Linux.
          'sources': [ '<@(chrome_interactive_ui_test_non_desktop_linux_sources)' ],
        }],
        ['OS=="linux" and chromeos==0 and use_ozone==0', {
          'sources': [ 'browser/ui/libgtk2ui/select_file_dialog_interactive_uitest.cc' ],
          'dependencies': [ '../build/linux/system.gyp:gtk2' ],
        }],
        ['use_ash==1', {
          'sources': [ '<@(chrome_interactive_ui_test_ash_sources)' ],
          'dependencies': [
            '../ash/ash.gyp:ash_interactive_ui_test_support',
          ],
        }],
        ['OS=="win"', {
          'dependencies': [
            'chrome.gyp:install_static_util',
          ],
        }],
        ['OS=="mac"', {
          'dependencies': [
            'chrome'
          ],
          # See comment about the same line in chrome/chrome_tests.gypi.
          'xcode_settings': {'OTHER_LDFLAGS': ['-Wl,-ObjC']},
          'conditions' : [
            # The browser window can be views or Cocoa on Mac. Test accordingly.
            ['mac_views_browser==1', {
              'sources': [ '<@(chrome_interactive_ui_test_views_non_mac_sources)' ],
              # Following tests needs a refactoring to works with mac_views.
              'sources!': [
                # Aura depended tests.
                'browser/ui/views/bookmarks/bookmark_bar_view_test.cc',
              ]
            }, {
              'sources': [ '<@(chrome_interactive_ui_test_cocoa_sources)' ],
            }],  # mac_views_browser==1
          ],
        }, {  # Non-Mac.
          'sources': [ '<@(chrome_interactive_ui_test_views_non_mac_sources)' ],
        }],
        ['notifications == 1', {
          # Common notifications tests.
          'sources': [
            '<@(chrome_interactive_ui_test_notifications_sources)',
          ],
          'conditions': [
            ['chromeos == 1', {
              'sources': [
                'browser/notifications/login_state_notification_blocker_chromeos_browsertest.cc',
              ],
            }, {
              # Non-ChromeOS notifications tests (ChromeOS does not use
              # cross-platform panels).
              'sources': [
                # Note: List duplicated in GN build.
                'browser/notifications/notification_interactive_uitest.cc',
                'browser/notifications/platform_notification_service_interactive_uitest.cc',
              ],
            }],
            ['OS=="android"', {
              'sources!': [
                # Note: List duplicated in GN build.

                # Android does not use the message center-based Notification system.
                'browser/notifications/message_center_notifications_browsertest.cc',

                # TODO(peter): Enable the Notification browser tests.
                'browser/notifications/notification_ui_browsertest.cc',
                'browser/notifications/platform_notification_service_ui_browsertest.cc',
              ]
            }],
          ],
        }],
        ['toolkit_views==1', {
          'sources': [ '<@(chrome_interactive_ui_test_views_sources)' ],
          'dependencies': [
            '../ui/views/controls/webview/webview_tests.gyp:webview_test_support',
            '../ui/views/views.gyp:views',
            '../ui/views/views.gyp:views_test_support',
          ],
          'conditions': [
            ['use_aura==1', {
              'dependencies': [
                '../ui/touch_selection/ui_touch_selection.gyp:ui_touch_selection',
              ],
            }],
          ],
        }],
        ['use_aura==0 or chromeos==1', {
          'sources!': [
            '../ui/views/corewm/desktop_capture_controller_unittest.cc',
          ],
        }],
        ['chromeos==1', {
          'dependencies': [
            '../ash/ash_resources.gyp:ash_resources',
            '../chromeos/chromeos.gyp:chromeos',
          ],
          'conditions': [
            ['disable_nacl==0 and disable_nacl_untrusted==0', {
              'dependencies': [
                '../native_client/src/trusted/service_runtime/linux/nacl_bootstrap.gyp:nacl_helper_bootstrap',
                '../components/nacl.gyp:nacl_helper',
                '../components/nacl_nonsfi.gyp:nacl_helper_nonsfi',
              ],
            }],
          ],
          'sources': [
            '<@(chrome_interactive_ui_test_chromeos_sources)',
          ],
          'sources!': [
            # Note: List duplicated in GN build.
            '../ui/views/widget/desktop_aura/desktop_window_tree_host_x11_interactive_uitest.cc',
            '../ui/views/widget/desktop_aura/x11_topmost_window_finder_interactive_uitest.cc',

            # Use only the _chromeos version on ChromeOS.
            'test/base/view_event_test_platform_part_default.cc',
          ],
        }],
        ['chromeos==1 and branding=="Chrome"', {
          'sources!': [
            # These tests are failing on official cros bots. crbug.com/431450.
            # Note: list duplicated in GN build.
            'browser/ui/views/bookmarks/bookmark_bar_view_test.cc',
          ],
        }],
        ['OS=="win"', {
          'include_dirs': [
            '../third_party/wtl/include',
          ],
          'dependencies': [
            '../third_party/isimpledom/isimpledom.gyp:isimpledom',
            '../ui/resources/ui_resources.gyp:ui_resources',
            'chrome.gyp:chrome_version_resources',
          ],
          'sources': [
            '<@(chrome_interactive_ui_test_win_sources)',

            # TODO:  It would be nice to have these pulled in
            # automatically from direct_dependent_settings in
            # their various targets (net.gyp:net_resources, etc.),
            # but that causes errors in other targets when
            # resulting .res files get referenced multiple times.
            '<(SHARED_INTERMEDIATE_DIR)/chrome_version/other_version.rc',
            '<(SHARED_INTERMEDIATE_DIR)/ui/resources/ui_unscaled_resources.rc',
          ],
          'msvs_settings': {
            'VCLinkerTool': {
              'conditions': [
                ['incremental_chrome_dll==1', {
                  'UseLibraryDependencyInputs': "true",
                }],
              ],
            },
          },
          # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
          'msvs_disabled_warnings': [ 4267, ],
        }],  # OS != "win"
        ['enable_app_list==1', {
          'sources': [ '<@(chrome_interactive_ui_test_app_list_sources)' ],
        }],
      ],  # conditions
    },
    {
      # GN version: //chrome/test/chromedriver:automation_client_lib
      'target_name': 'automation_client_lib',
      'type': 'static_library',
      'hard_dependency': 1,
      'dependencies': [
        '../base/base.gyp:base',
        '../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '../net/net.gyp:net',
        '../third_party/zlib/zlib.gyp:minizip',
        '../third_party/zlib/zlib.gyp:zlib',
        '../ui/accessibility/accessibility.gyp:ax_gen',
        '../ui/base/ui_base.gyp:ui_base',
        '../ui/gfx/gfx.gyp:gfx',
        '../ui/gfx/gfx.gyp:gfx_geometry',
        '../url/url.gyp:url_lib',
      ],
      'include_dirs': [
        '..',
        '<(SHARED_INTERMEDIATE_DIR)',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(SHARED_INTERMEDIATE_DIR)',
        ],
      },
      'sources': [
        '<@(chrome_automation_client_lib_sources)',
        '<(SHARED_INTERMEDIATE_DIR)/chrome/test/chromedriver/chrome/embedded_automation_extension.cc',
        '<(SHARED_INTERMEDIATE_DIR)/chrome/test/chromedriver/chrome/embedded_automation_extension.h',
        '<(SHARED_INTERMEDIATE_DIR)/chrome/test/chromedriver/chrome/js.cc',
        '<(SHARED_INTERMEDIATE_DIR)/chrome/test/chromedriver/chrome/js.h',
        '<(SHARED_INTERMEDIATE_DIR)/chrome/test/chromedriver/chrome/user_data_dir.cc',
        '<(SHARED_INTERMEDIATE_DIR)/chrome/test/chromedriver/chrome/user_data_dir.h',
      ],
      'actions': [
        {
          # GN version: //chrome/test/chromedriver:embed_js_in_cpp
          'action_name': 'embed_js_in_cpp',
          'inputs': [
            'test/chromedriver/cpp_source.py',
            'test/chromedriver/embed_js_in_cpp.py',
            'test/chromedriver/js/add_cookie.js',
            'test/chromedriver/js/call_function.js',
            'test/chromedriver/js/execute_async_script.js',
            'test/chromedriver/js/focus.js',
            'test/chromedriver/js/get_element_region.js',
            'test/chromedriver/js/is_option_element_toggleable.js',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/chrome/test/chromedriver/chrome/js.cc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/test/chromedriver/chrome/js.h',
          ],
          'action': [ 'python',
                      'test/chromedriver/embed_js_in_cpp.py',
                      '--directory',
                      '<(SHARED_INTERMEDIATE_DIR)/chrome/test/chromedriver/chrome',
                      'test/chromedriver/js/add_cookie.js',
                      'test/chromedriver/js/call_function.js',
                      'test/chromedriver/js/execute_async_script.js',
                      'test/chromedriver/js/focus.js',
                      'test/chromedriver/js/get_element_region.js',
                      'test/chromedriver/js/is_option_element_toggleable.js',
          ],
          'message': 'Generating sources for embedding js in chromedriver',
        },
        {
          # GN version: //chrome/test/chromedriver:embed_user_data_dir_in_cpp
          'action_name': 'embed_user_data_dir_in_cpp',
          'inputs': [
            'test/chromedriver/cpp_source.py',
            'test/chromedriver/embed_user_data_dir_in_cpp.py',
            'test/chromedriver/chrome/preferences.txt',
            'test/chromedriver/chrome/local_state.txt',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/chrome/test/chromedriver/chrome/user_data_dir.cc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/test/chromedriver/chrome/user_data_dir.h',
          ],
          'action': [ 'python',
                      'test/chromedriver/embed_user_data_dir_in_cpp.py',
                      '--directory',
                      '<(SHARED_INTERMEDIATE_DIR)/chrome/test/chromedriver/chrome',
                      'test/chromedriver/chrome/preferences.txt',
                      'test/chromedriver/chrome/local_state.txt',
          ],
          'message': 'Generating sources for embedding user data dir in chromedriver',
        },
        {
          # GN version: //chrome/test/chromedriver:embed_extension_in_cpp
          'action_name': 'embed_extension_in_cpp',
          'inputs': [
            'test/chromedriver/cpp_source.py',
            'test/chromedriver/embed_extension_in_cpp.py',
            'test/chromedriver/extension/background.js',
            'test/chromedriver/extension/manifest.json',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/chrome/test/chromedriver/chrome/embedded_automation_extension.cc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/test/chromedriver/chrome/embedded_automation_extension.h',
          ],
          'action': [ 'python',
                      'test/chromedriver/embed_extension_in_cpp.py',
                      '--directory',
                      '<(SHARED_INTERMEDIATE_DIR)/chrome/test/chromedriver/chrome',
                      'test/chromedriver/extension/background.js',
                      'test/chromedriver/extension/manifest.json',
          ],
          'message': 'Generating sources for embedding automation extension',
        },
      ],
      # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
      'msvs_disabled_warnings': [ 4267, ],
    },
    {
      # GN version: //chrome/test/chromedriver:lib
      'target_name': 'chromedriver_lib',
      'type': 'static_library',
      'hard_dependency': 1,
      'dependencies': [
        'automation_client_lib',
        'common_constants.gyp:version_header',
        '../base/base.gyp:base',
        '../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '../crypto/crypto.gyp:crypto',
        '../net/net.gyp:http_server',
        '../net/net.gyp:net',
        '../third_party/zlib/google/zip.gyp:zip',
        '../ui/base/ui_base.gyp:ui_base',
        '../ui/events/events.gyp:dom_keycode_converter',
        '../ui/events/events.gyp:events_base',
        '../ui/events/ozone/events_ozone.gyp:events_ozone_layout',
        '../ui/gfx/gfx.gyp:gfx',
        '../ui/gfx/gfx.gyp:gfx_geometry',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        '<@(chrome_driver_lib_sources)',
        '<(SHARED_INTERMEDIATE_DIR)/chrome/test/chromedriver/version.cc',
        '<(SHARED_INTERMEDIATE_DIR)/chrome/test/chromedriver/version.h',
      ],
      'actions': [
        {
          # GN version: //chrome/test/chromedriver:embed_version_in_cpp
          'action_name': 'embed_version_in_cpp',
          'inputs': [
            'test/chromedriver/cpp_source.py',
            'test/chromedriver/embed_version_in_cpp.py',
            'test/chromedriver/VERSION',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/chrome/test/chromedriver/version.cc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/test/chromedriver/version.h',
          ],
          'action': [ 'python',
                      'test/chromedriver/embed_version_in_cpp.py',
                      '--version-file',
                      'test/chromedriver/VERSION',
                      '--directory',
                      '<(SHARED_INTERMEDIATE_DIR)/chrome/test/chromedriver',
          ],
          'message': 'Generating version info',
        },
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(SHARED_INTERMEDIATE_DIR)',
        ],
      },
      'conditions': [
        ['use_x11==1', {
          'dependencies': [
            '../build/linux/system.gyp:x11',
            '../ui/events/keycodes/events_keycodes.gyp:keycodes_x11',
            '../ui/gfx/x/gfx_x11.gyp:gfx_x11',
          ]
        }]
      ],
      # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
      'msvs_disabled_warnings': [ 4267, ],
    },
    {
      # GN version: //chrome/test/chromedriver
      'target_name': 'chromedriver',
      'type': 'executable',
      'dependencies': [
        'chromedriver_lib',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'test/chromedriver/server/chromedriver_server.cc',
      ],
      # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
      'msvs_disabled_warnings': [ 4267, ],
    },
    {
      # GN version: //chrome/test/chromedriver:chromedriver_unittests
      'target_name': 'chromedriver_unittests',
      'type': 'executable',
      'dependencies': [
        'chromedriver_lib',
        '../base/base.gyp:base',
        '../base/base.gyp:run_all_unittests',
        '../net/net.gyp:http_server',
        '../net/net.gyp:net',
        '../testing/gtest.gyp:gtest',
        '../ui/base/ui_base.gyp:ui_base',
        '../ui/gfx/gfx.gyp:gfx',
        '../ui/gfx/gfx.gyp:gfx_geometry',
      ],
      'include_dirs': [
        '..,'
      ],
      'sources': [
        '<@(chrome_driver_unittests_sources)',
      ],
      # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
      'msvs_disabled_warnings': [ 4267, ],
    },
    # ChromeDriver tests that aren't run on the main buildbot. Available
    # as an optional test type on trybots.
    {
      # GN version: //chrome/test/chromedriver:chromedriver_tests
      'target_name': 'chromedriver_tests',
      'type': 'executable',
      'dependencies': [
        'chromedriver_lib',
        '../base/base.gyp:base',
        '../base/base.gyp:run_all_unittests',
        '../net/net.gyp:http_server',
        '../net/net.gyp:net',
        '../testing/gtest.gyp:gtest',
        '../url/url.gyp:url_lib',
      ],
      'include_dirs': [
        '..,'
      ],
      'sources': [
        '<@(chrome_driver_tests_sources)',
      ],
      # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
      'msvs_disabled_warnings': [ 4267, ],
    },
    {
      # Executable that runs each browser test in a new process.
      'target_name': 'browser_tests',
      'type': 'executable',
      'dependencies': [
        'browser',
        'chrome_features.gyp:chrome_common_features',
        'chrome_resources.gyp:browser_tests_pak',
        'chrome_resources.gyp:chrome_resources',
        'chrome_resources.gyp:chrome_strings',
        'chrome_resources.gyp:packed_extra_resources',
        'chrome_resources.gyp:packed_resources',
        'common/extensions/api/api.gyp:chrome_api',
        'renderer',
        'test_support_common',
        'test_support_sync_integration',
        'test_support_ui',
        '../base/base.gyp:base',
        '../base/base.gyp:base_i18n',
        '../base/base.gyp:test_support_base',
        '../components/components.gyp:autofill_content_risk_proto',
        '../components/components.gyp:autofill_content_test_support',
        '../components/components.gyp:captive_portal_test_support',
        '../components/components.gyp:certificate_reporting',
        '../components/components.gyp:dom_distiller_content_browser',
        '../components/components.gyp:dom_distiller_content_renderer',
        '../components/components.gyp:dom_distiller_test_support',
        '../components/components.gyp:guest_view_test_support',
        '../components/components.gyp:history_core_test_support',
        '../components/components.gyp:safe_browsing_db',
        '../components/components.gyp:ssl_config',
        '../components/components.gyp:subresource_filter_core_browser_test_support',
        '../components/components.gyp:subresource_filter_core_common_test_support',
        '../components/components.gyp:subresource_filter_content_browser',
        '../components/components.gyp:test_database_manager',
        '../components/components.gyp:translate_core_common',
        '../components/components.gyp:zoom_test_support',
        '../components/components_resources.gyp:components_resources',
        '../components/components_strings.gyp:components_strings',
        '../content/content.gyp:common_features',
        '../content/content.gyp:feature_h264_with_openh264_ffmpeg',
        '../content/content_shell_and_tests.gyp:content_browser_test_base',
        '../crypto/crypto.gyp:crypto_test_support',
        '../device/bluetooth/bluetooth.gyp:device_bluetooth_mocks',
        '../device/serial/serial.gyp:device_serial_test_util',
        '../device/usb/usb.gyp:device_usb_mocks',
        '../extensions/common/api/api.gyp:extensions_api',
        '../google_apis/google_apis.gyp:google_apis_test_support',
        '../media/cast/cast.gyp:cast_test_utility',
        '../media/media.gyp:media',
        '../net/net.gyp:net',
        '../net/net.gyp:net_test_support',
        '../sdch/sdch.gyp:sdch',
        '../skia/skia.gyp:skia',
        '../components/sync.gyp:sync',
        '../components/sync.gyp:test_support_sync_api',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        '../testing/perf/perf_test.gyp:*',
        '../third_party/cacheinvalidation/cacheinvalidation.gyp:cacheinvalidation',
        '../third_party/icu/icu.gyp:icui18n',
        '../third_party/icu/icu.gyp:icuuc',
        '../third_party/leveldatabase/leveldatabase.gyp:leveldatabase',
        '../third_party/libaddressinput/libaddressinput.gyp:libaddressinput',
        '../third_party/webrtc/modules/modules.gyp:desktop_capture',
        '../third_party/widevine/cdm/widevine_cdm.gyp:widevine_cdm_version_h',
        '../ui/accessibility/accessibility.gyp:accessibility_test_support',
        '../ui/compositor/compositor.gyp:compositor_test_support',
        '../ui/resources/ui_resources.gyp:ui_resources',
        '../ui/web_dialogs/web_dialogs.gyp:web_dialogs_test_support',
        '../v8/src/v8.gyp:v8',
        # Runtime dependencies
        '../ppapi/ppapi_internal.gyp:power_saver_test_plugin',
        '../ppapi/ppapi_internal.gyp:ppapi_tests',
        '../remoting/remoting.gyp:remoting_browser_test_resources',
        '../remoting/remoting.gyp:remoting_webapp_unittests',
        '../third_party/mesa/mesa.gyp:osmesa',
        '../third_party/widevine/cdm/widevine_cdm.gyp:widevine_test_license_server',
      ],
      'include_dirs': [
        '..',
        '<(SHARED_INTERMEDIATE_DIR)',
      ],
      'defines': [
        'HAS_OUT_OF_PROC_TEST_RUNNER',
      ],
      'sources': [
        '<@(chrome_browser_tests_desktop_only_sources)',
        '<@(chrome_browser_tests_extensions_sources)',
        '<@(chrome_browser_tests_sources)',
        '<@(chrome_browser_tests_webui_js_sources)',
        '<@(chrome_browser_extensions_test_support_sources)',
        'test/base/browser_tests_main.cc',
      ],
      'rules': [
        {
          'rule_name': 'js2webui',
          'extension': 'js',
          'msvs_external_rule': 1,
          'variables': {
            'conditions': [
              ['v8_use_external_startup_data==1', {
                'external_v8': 'y',
              }, {
                'external_v8': 'n',
              }],
            ],
          },
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
            '--external', '<(external_v8)',
            'webui',
            '<(RULE_INPUT_PATH)',
            'chrome/<(RULE_INPUT_DIRNAME)/<(RULE_INPUT_ROOT).js',
            '<@(_outputs)',
          ],
        },
      ],
      'msvs_settings': {
        'VCLinkerTool': {
          'conditions': [
            ['incremental_chrome_dll==1', {
              'UseLibraryDependencyInputs': "true",
            }],
          ],
        },
      },
      'conditions': [
        ['chromeos==1', {
          'dependencies': [
            '../components/components.gyp:arc_test_support',
            '../third_party/boringssl/boringssl.gyp:boringssl',
          ]
        }, {
          'conditions': [
            ['OS == "linux" or OS == "win"', {
              'sources': [
                'browser/ui/views/ime/ime_warning_bubble_browsertest.cc',
                'browser/ui/views/ime/ime_window_browsertest.cc',
              ],
            }],
            ['OS=="win"', {
              'dependencies': [
                'chrome.gyp:install_static_util',
              ],
            }],
          ]
        }],
        ['enable_wifi_display==1', {
          'sources': [
            '<@(chrome_browser_tests_display_source_apitest)',
          ],
        }],
        ['enable_one_click_signin==0', {
          'sources!': [
            'browser/ui/sync/one_click_signin_links_delegate_impl_browsertest.cc',
          ]
        }],
        ['disable_nacl==0', {
          'sources':[
            'browser/chrome_service_worker_browsertest.cc',
            'browser/extensions/extension_nacl_browsertest.cc',
            'browser/nacl_host/test/gdb_debug_stub_browsertest.cc',
          ],
          'dependencies': [
            'test/data/nacl/nacl_test_data.gyp:pnacl_url_loader_test',
          ],
          'conditions': [
            ['disable_nacl_untrusted==0', {
              'sources': [
                'test/nacl/nacl_browsertest.cc',
                'test/nacl/nacl_browsertest_uma.cc',
                'test/nacl/nacl_browsertest_util.cc',
                'test/nacl/nacl_browsertest_util.h',
                'test/nacl/pnacl_header_test.cc',
                'test/nacl/pnacl_header_test.h',
              ],
              'dependencies': [
                'test/data/nacl/nacl_test_data.gyp:*',
                '../ppapi/native_client/native_client.gyp:nacl_irt',
                '../ppapi/ppapi_nacl.gyp:ppapi_nacl_tests',
                '../ppapi/tests/extensions/extensions.gyp:ppapi_tests_extensions_background_keepalive',
                '../ppapi/tests/extensions/extensions.gyp:ppapi_tests_extensions_load_unload',
                '../ppapi/tests/extensions/extensions.gyp:ppapi_tests_extensions_media_galleries',
                '../ppapi/tests/extensions/extensions.gyp:ppapi_tests_extensions_multicast_permissions',
                '../ppapi/tests/extensions/extensions.gyp:ppapi_tests_extensions_no_socket_permissions',
                '../ppapi/tests/extensions/extensions.gyp:ppapi_tests_extensions_packaged_app',
                '../ppapi/tests/extensions/extensions.gyp:ppapi_tests_extensions_popup',
                '../ppapi/tests/extensions/extensions.gyp:ppapi_tests_extensions_socket_permissions',
              ],
              'conditions': [
                ['chromeos==1', {
                  'sources': [
                    '../third_party/liblouis/nacl_wrapper/liblouis_wrapper_browsertest.cc',
                  ],
                  'dependencies': [
                    'browser_chromeos',
                    '../third_party/liblouis/liblouis_nacl.gyp:liblouis_test_data',
                  ],
                }],
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
            ['OS=="win"', {
              # TODO(halyavin) NaCl on Windows can't open debug stub socket
              # in browser process as needed by this test.
              # See http://crbug.com/157312.
              'sources!': [
                'browser/nacl_host/test/gdb_debug_stub_browsertest.cc',
              ],
              'dependencies': [
                'chrome.gyp:chrome_nacl_win64',
              ],
            }],
            ['OS=="linux"', {
              'dependencies': [
                '../native_client/src/trusted/service_runtime/linux/nacl_bootstrap.gyp:nacl_helper_bootstrap',
                '../components/nacl.gyp:nacl_helper',
                '../components/nacl_nonsfi.gyp:nacl_helper_nonsfi',
              ],
            }],
            ['chromeos==0', {
              'sources!': [
                'test/data/chromeos/oobe_webui_browsertest.js',
              ],
            }],
          ],
        }],
        ['debug_devtools==1', {
          'defines': [
            'DEBUG_DEVTOOLS=1',
          ],
        }],
        ['use_ash==1', {
          'sources': [ '<@(chrome_browser_tests_ash_sources)' ],
          'dependencies': [
            '../ash/ash.gyp:ash_test_support',
          ],
          'conditions': [
            ['enable_app_list==1', {
              'sources': [
                '<@(chrome_browser_app_list_ash_test_support_sources)'
              ],
              'dependencies': [
                '../ui/app_list/presenter/app_list_presenter.gyp:app_list_presenter_test_support',
              ],
            }],
          ],
        }],
        ['use_aura==1 or toolkit_views==1', {
          'dependencies': [
            '../ui/events/events.gyp:events_test_support',
          ],
        }],
        ['chromeos == 1', {
          'sources': [ '<@(chrome_browser_tests_chromeos_sources)' ],
          'sources!': [
            # GN version: //chrome/test/browser_tests
            '../apps/load_and_launch_browsertest.cc',
            'browser/policy/policy_startup_browsertest.cc',
            'browser/printing/cloud_print/test/cloud_print_policy_browsertest.cc',
            'browser/printing/cloud_print/test/cloud_print_proxy_process_browsertest.cc',
            # chromeos does not support profile list avatar menu
            'browser/profiles/profile_list_desktop_browsertest.cc',
            'browser/service_process/service_process_control_browsertest.cc',
            # bookmark sign in promo not used on chromeos
            'browser/ui/bookmarks/bookmark_bubble_sign_in_delegate_browsertest.cc',
            # inline login UI is disabled on chromeos
            'browser/ui/webui/signin/inline_login_ui_browsertest.cc',
            # chromeos does not use the desktop user manager
            'browser/ui/webui/signin/user_manager_ui_browsertest.cc',

            # GN version: //chrome/test/browser_tests_js_webui
            # chromeos does not use the desktop user manager
            'test/data/webui/md_user_manager/user_manager_browsertest.js'
          ],
          'dependencies': [
            '../build/linux/system.gyp:dbus',
            '../chromeos/ime/input_method.gyp:gencode',
            '../components/components.gyp:drive_test_support',
            '../dbus/dbus.gyp:dbus_test_support',
            '../ui/login/login.gyp:login_resources',
          ],
        }, {  # Non-ChromeOS
          'sources!': [
            # GN version: //chrome/test/browser_tests
            'browser/extensions/api/enterprise_device_attributes/enterprise_device_attributes_apitest.cc',
            'browser/extensions/api/enterprise_platform_keys/enterprise_platform_keys_apitest_nss.cc',
            'browser/extensions/api/platform_keys/platform_keys_apitest_nss.cc',
            'browser/extensions/api/terminal/terminal_private_apitest.cc',
            'browser/invalidation/profile_invalidation_provider_factory_browsertest.cc',
            'browser/net/nss_context_chromeos_browsertest.cc',
            'browser/ui/ash/keyboard_controller_browsertest.cc',
            'browser/ui/views/select_file_dialog_extension_browsertest.cc',
            'test/data/webui/certificate_viewer_dialog_test.js',
            'test/data/webui/certificate_viewer_ui_test-inl.h',
            'test/data/webui/settings/bluetooth_page_browsertest_chromeos.js',
            'test/data/webui/settings/easy_unlock_browsertest_chromeos.js',
          ],
          'conditions': [
            ['OS=="linux" or OS=="win"', {
              'sources': [
                'browser/ui/views/ime/input_ime_apitest_nonchromeos.cc',
              ]
            }],
          ]
        }],
        ['enable_web_speech==1', {
          'sources': [ '<@(chrome_browser_tests_speech_sources)' ],
        }],
        # TODO(nparker) enable tests for safe_browsing==2.
        ['safe_browsing==1', {
          'sources': [ '<@(chrome_browser_tests_full_safe_browsing_sources)' ],
          'dependencies': [
            '../components/components.gyp:safe_browsing_metadata_proto',
          ],
        }],
        ['enable_captive_portal_detection==1', {
          'sources': [ 'browser/captive_portal/captive_portal_browsertest.cc' ],
        }],
        ['enable_webrtc==0', {
          'sources!': [
            'browser/extensions/api/webrtc_audio_private/webrtc_audio_private_browsertest.cc',
            'browser/extensions/api/webrtc_logging_private/webrtc_event_log_apitest.cc',
            'browser/extensions/api/webrtc_logging_private/webrtc_logging_private_apitest.cc',
            'browser/media/webrtc_apprtc_browsertest.cc',
            'browser/media/webrtc_audio_quality_browsertest.cc',
            'browser/media/webrtc_browsertest.cc',
            'browser/media/webrtc_disable_encryption_flag_browsertest.cc',
            'browser/media/webrtc_getmediadevices_browsertest.cc',
            'browser/media/webrtc_perf_browsertest.cc',
            'browser/media/webrtc_simulcast_browsertest.cc',
            'browser/media/webrtc_video_quality_browsertest.cc',
            'browser/media/webrtc_webcam_browsertest.cc',
         ],
        }],
        ['OS=="win"', {
          'sources': [
            '<(SHARED_INTERMEDIATE_DIR)/chrome_version/other_version.rc',
            '<(SHARED_INTERMEDIATE_DIR)/ui/resources/ui_unscaled_resources.rc',
          ],
          'include_dirs': [
            '<(DEPTH)/third_party/wtl/include',
          ],
          'dependencies': [
            'chrome_version_resources',
          ],
        }, { # else: OS != "win"
          'sources!': [
            'app/chrome_dll.rc',
            'app/chrome_dll_resource.h',
            'app/chrome_version.rc.version',
          ],
        }],
        ['chromeos==0 and use_ash==1', {
          'sources!': [
            # On Windows and Linux, we currently don't support enough of the
            # ash environment to run these unit tests.
            #
            # TODO: enable these on windows and linux.
            'browser/ui/ash/accelerator_commands_browsertest.cc',
            'browser/ui/ash/accelerator_controller_browsertest.cc',
            'browser/ui/ash/launcher/chrome_launcher_controller_impl_browsertest.cc',
            'browser/ui/ash/launcher/launcher_favicon_loader_browsertest.cc',
            'browser/ui/ash/shelf_browsertest.cc',
            'browser/ui/views/frame/browser_non_client_frame_view_ash_browsertest.cc',
          ],
        }],
        ['OS=="linux"', {
          'dependencies': [
            '../build/linux/system.gyp:nss',
          ],
        }],
        ['OS=="mac"', {
          # TODO(mark): We really want this for all non-static library
          # targets, but when we tried to pull it up to the common.gypi
          # level, it broke other things like the ui and startup tests. *shrug*
          'xcode_settings': {
            'OTHER_LDFLAGS': [
              '-Wl,-ObjC',
            ],
          },
          # Other platforms only need
          # chrome_resources.gyp:{packed_extra_resources,packed_resources},
          # and can build this target standalone much faster.
          'dependencies': [
            'app_mode_app_support',
            'chrome',
            '../components/components.gyp:breakpad_stubs',
            '../third_party/ocmock/ocmock.gyp:ocmock',
          ],
          'sources': [ '<@(chrome_browser_tests_mac_sources)' ],
          'sources!': [
            # TODO(groby): This test depends on hunspell and we cannot run it on
            # Mac, which does not use hunspell by default.
            'browser/spellchecker/spellcheck_service_browsertest.cc',

            # TODO(rouslan): This test depends on the custom dictionary UI,
            # which is disabled on Mac.
            'browser/ui/webui/options/edit_dictionary_browsertest.js',
            # TODO(rouslan): This test depends on hunspell and we cannot run it
            # on Mac, which does use hunspell by default.
            'browser/ui/webui/options/language_options_dictionary_download_browsertest.js',
            # This test depends on GetCommandLineForRelaunch, which is not
            # available on Mac.
            'browser/printing/cloud_print/test/cloud_print_policy_browsertest.cc',
            # ProcessSingletonMac doesn't do anything.
            'browser/process_singleton_browsertest.cc',
            # single-process mode hangs on Mac sometimes because of multiple UI
            # message loops. See 306348
            'renderer/safe_browsing/phishing_classifier_browsertest.cc',
            'renderer/safe_browsing/phishing_classifier_delegate_browsertest.cc',
            # This tests the language options UI features that do not exist on
            # Mac.
            'browser/ui/webui/options/multilanguage_options_webui_browsertest.js',
          ],
          'conditions': [
            # The browser window can be views or Cocoa on Mac. Test accordingly.
            ['mac_views_browser==1', {
              'sources': [ '<@(chrome_browser_tests_views_non_mac_sources)' ],
            }, {
              'sources': [ '<@(chrome_browser_tests_cocoa_sources)' ],
            }],
          ],
        }],  # OS=="mac"
        ['OS=="mac" or OS=="win"', {
          'sources': [
            'browser/extensions/api/networking_private/networking_private_apitest.cc',
            'browser/extensions/api/networking_private/networking_private_service_client_apitest.cc',
            'browser/media_galleries/fileapi/itunes_data_provider_browsertest.cc',
            'browser/media_galleries/fileapi/picasa_data_provider_browsertest.cc',
          ],
          'dependencies': [
            '../components/components.gyp:wifi_test_support',
          ],
        }],
        ['OS=="linux" or OS=="win"', {
            'sources': [ '<@(chrome_browser_tests_non_mac_desktop_sources)' ],
        }],
        ['os_posix == 0 or chromeos == 1', {
          'sources!': [
            'common/time_format_browsertest.cc',
          ],
        }],
        ['OS=="android"', {
          'sources!': [
            'browser/policy/cloud/component_cloud_policy_browsertest.cc',
            'browser/prefs/pref_hash_browsertest.cc',
            'browser/ui/bookmarks/bookmark_bubble_sign_in_delegate_browsertest.cc',
          ],
        }],
        ['chromeos == 1', {
          'sources': [
            'browser/extensions/api/networking_private/networking_private_apitest.cc',
            'browser/extensions/api/networking_private/networking_private_chromeos_apitest.cc',
          ],
        }],
        ['OS=="win" or OS=="mac" or (OS=="linux" and chromeos==0)', {
            'sources': [ '<@(chrome_browser_tests_non_mobile_non_cros_sources)' ],
        }],
        ['toolkit_views==1', {
          'sources': [ '<@(chrome_browser_tests_views_sources)' ],
          'dependencies': [
            '../ui/views/views.gyp:views',
          ],
        }],
        ['toolkit_views==1 and OS!="mac"', {
          'sources': [ '<@(chrome_browser_tests_views_non_mac_sources)' ],
        }],
        ['toolkit_views==1 and (OS!="mac" or mac_views_browser==1) and chromeos == 0', {
          'sources': [ '<@(chrome_browser_tests_views_non_cros_sources)' ],
        }],
        ['OS=="linux" or OS=="win"', {
          'sources': [ '<@(chrome_browser_tests_non_mac_desktop_sources)' ],
        }],
        ['OS=="ios"', {
          'sources!': [
            # TODO(dbeam): log webui URLs on iOS and test them.
            'browser/ui/webui/log_web_ui_url_browsertest.cc',
          ],
        }, { # else: OS != "ios"
          'dependencies': [
            '../mojo/mojo_base.gyp:mojo_common_lib',
          ],
        }],
        ['enable_app_list==1', {
          'sources': [ '<@(chrome_browser_tests_app_list_sources)' ],
          'conditions': [
            ['OS=="mac"', {
              'sources!': [
                # This assumes the AppList is views-based, but Mac only links
                # browser parts for the Cocoa implementation.
                'browser/ui/app_list/app_list_service_views_browsertest.cc',
              ],
            }],
          ],
        }, {
          'sources!': [ 'browser/ui/webui/app_list/start_page_browsertest.js' ],
        }],
        ['enable_service_discovery==1', {
          'sources': [ '<@(chrome_browser_tests_service_discovery_sources)' ],
        }],
        ['enable_supervised_users==1', {
          'sources': [ '<@(chrome_browser_tests_supervised_user_sources)' ],
        }],
        ['enable_pepper_cdms==1', {
          'dependencies': [
            # Runtime dependencies.
            '../media/media.gyp:clearkeycdmadapter',
            '../third_party/widevine/cdm/widevine_cdm.gyp:widevinecdmadapter',
          ],
        }],
        ['enable_print_preview==0', {
          'sources!': [
            'browser/extensions/api/cloud_print_private/cloud_print_private_apitest.cc',
            'browser/printing/cloud_print/test/cloud_print_policy_browsertest.cc',
            'browser/printing/cloud_print/test/cloud_print_proxy_process_browsertest.cc',
            'browser/printing/print_preview_dialog_controller_browsertest.cc',
            'browser/printing/print_preview_pdf_generated_browsertest.cc',
            'browser/service_process/service_process_control_browsertest.cc',
            'browser/ui/webui/print_preview/print_preview_ui_browsertest.cc',
            'test/data/webui/print_preview.cc',
            'test/data/webui/print_preview.h',
            'test/data/webui/print_preview.js',
          ],
        }],
        ['enable_media_router==1', {
          'sources': [ '<@(chrome_browser_tests_media_router_sources)' ],
          'dependencies': [
            'browser/media/router/media_router.gyp:media_router_test_support',
            'test/media_router/media_router_tests.gypi:media_router_integration_test_files'
          ],
        }],
        ['enable_mdns==1', {
          'sources' : [
            'browser/extensions/api/gcd_private/gcd_private_apitest.cc',
            'browser/ui/webui/local_discovery/local_discovery_ui_browsertest.cc',
          ]
        }],
        [ 'use_brlapi==0', {
          'sources!': [
            'browser/extensions/api/braille_display_private/braille_display_private_apitest.cc'
            ]
        }],
        ['branding=="Chrome"', {
          'sources!': [
            # crbug.com/230471
            'test/data/webui/accessibility_audit_browsertest.js',
            # These tests depend on single process mode, which is disabled in
            # official builds.
            'renderer/safe_browsing/phishing_classifier_browsertest.cc',
            'renderer/safe_browsing/phishing_classifier_delegate_browsertest.cc',
          ]
        }],
        ['OS=="android" or OS=="ios"', {
          'dependencies!': [
            '../third_party/libaddressinput/libaddressinput.gyp:libaddressinput',
          ],
        }],
        ['remoting==1', {
          'sources': [ '<@(chrome_browser_tests_remoting_sources)' ],
          'dependencies': [
            '../remoting/remoting.gyp:remoting_webapp',
          ]
        }],
        ['use_x11==1', {
          'dependencies': [
            '../tools/xdisplaycheck/xdisplaycheck.gyp:xdisplaycheck',
          ],
        }],
      ],  # conditions
    },  # target browser_tests
    {
      # Executable that runs each perf browser test in a new process.
      'target_name': 'performance_browser_tests',
      'type': 'executable',
      'dependencies': [
        'browser',
        'chrome_features.gyp:chrome_common_features',
        'chrome_resources.gyp:chrome_resources',
        'chrome_resources.gyp:chrome_strings',
        'chrome_resources.gyp:packed_extra_resources',
        'chrome_resources.gyp:packed_resources',
        'renderer',
        'test_support_common',
        'test_support_ui',
        '../base/base.gyp:base',
        '../base/base.gyp:base_i18n',
        '../base/base.gyp:test_support_base',
        '../components/components.gyp:autofill_content_test_support',
        '../content/content_shell_and_tests.gyp:content_browser_test_base',
        '../media/cast/cast.gyp:cast_test_utility',
        '../net/net.gyp:net',
        '../net/net.gyp:net_test_support',
        '../skia/skia.gyp:skia',
        '../components/sync.gyp:sync',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        '../testing/perf/perf_test.gyp:*',
        '../third_party/icu/icu.gyp:icui18n',
        '../third_party/icu/icu.gyp:icuuc',
        '../third_party/leveldatabase/leveldatabase.gyp:leveldatabase',
        '../v8/src/v8.gyp:v8',
        # Runtime dependencies
        '../third_party/mesa/mesa.gyp:osmesa',
      ],
      'include_dirs': [
        '..',
      ],
      'defines': [
        'HAS_OUT_OF_PROC_TEST_RUNNER',
      ],
      'sources': [
        '<@(performance_browser_tests_sources)',
      ],
      'conditions': [
        ['OS=="win"', {
          'sources': [
            '<(SHARED_INTERMEDIATE_DIR)/chrome_version/other_version.rc',
            '<(SHARED_INTERMEDIATE_DIR)/ui/resources/ui_unscaled_resources.rc',
          ],
          'include_dirs': [
            '<(DEPTH)/third_party/wtl/include',
          ],
          'dependencies': [
            'chrome_version_resources',
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
            'app/chrome_dll.rc',
            'app/chrome_dll_resource.h',
            'app/chrome_version.rc.version',
          ],
        }],
        ['use_x11==1', {
          'dependencies': [
            '../tools/xdisplaycheck/xdisplaycheck.gyp:xdisplaycheck',
          ],
        }],
        ['OS=="linux"', {
          'dependencies': [
            '../build/linux/system.gyp:nss',
          ],
        }],
        ['OS=="mac"', {
          # TODO(mark): We really want this for all non-static library
          # targets, but when we tried to pull it up to the common.gypi
          # level, it broke other things like the ui and startup tests. *shrug*
          'xcode_settings': {
            'OTHER_LDFLAGS': [
              '-Wl,-ObjC',
            ],
          },
          # Other platforms only need
          # chrome_resources.gyp:{packed_extra_resources,packed_resources},
          # and can build this target standalone much faster.
          'dependencies': [
            'chrome',
            '../components/components.gyp:breakpad_stubs',
          ],
        }, {  # OS!="mac"
          'sources!': [
            'test/perf/mach_ports_performancetest.cc',
          ],
        }],
      ],  # conditions
    },  # target performance_browser_tests
    {
      # GN version: //chrome/test:sync_integration_test_support
      'target_name': 'test_support_sync_integration',
      'type': 'static_library',
      'dependencies': [
        'browser',
        'chrome',
        'test_support_common',
        '../base/base.gyp:base',
        '../components/components.gyp:invalidation_impl',
        '../components/components.gyp:invalidation_test_support',
        '../content/content_shell_and_tests.gyp:content_browser_test_base',
        '../net/net.gyp:net',
        '../skia/skia.gyp:skia',
        '../components/sync.gyp:sync',
        '../components/sync.gyp:test_support_sync_fake_server',
        '../components/sync.gyp:test_support_sync_testserver',
      ],
      'include_dirs': [
        '..',
        '<(INTERMEDIATE_DIR)',
        '<(protoc_out_dir)',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '..',
          '<(INTERMEDIATE_DIR)',
          '<(protoc_out_dir)',
        ],
      },
      'export_dependent_settings': [
        'browser',
      ],
      'sources': [
        '<@(test_support_sync_integration_sources)',
      ],
      'conditions': [
        ['OS=="mac"', {
          # Dictionary sync is disabled on Mac.
          # Note: this list is duplicated in the GN build.
          'sources!': [
            'browser/sync/test/integration/dictionary_helper.cc',
            'browser/sync/test/integration/dictionary_helper.h',
            'browser/sync/test/integration/dictionary_load_observer.cc',
            'browser/sync/test/integration/dictionary_load_observer.h',
          ],
        }],
        ['chromeos==0', {
          # Note: this list is duplicated in the GN build.
          'sources!': [
	    'browser/sync/test/integration/sync_arc_package_helper.cc',
            'browser/sync/test/integration/sync_arc_package_helper.h',
            'browser/sync/test/integration/wifi_credentials_helper.cc',
            'browser/sync/test/integration/wifi_credentials_helper.h',
            # 'browser/sync/test/integration/wifi_credentials_helper_chromeos.cc',
            # 'browser/sync/test/integration/wifi_credentials_helper_chromeos.h',
          ],
        }],
	['chromeos==1', {
          'dependencies': [
	    '../components/components.gyp:arc',
            '../components/components.gyp:arc_test_support',
          ],
        }],
        ['enable_app_list==1', {
          'dependencies' : [
            '../ui/app_list/app_list.gyp:app_list_test_support',
          ],
        }, {
          # Note: this list is duplicated in the GN build.
          'sources!': [
            'browser/sync/test/integration/sync_app_list_helper.cc',
            'browser/sync/test/integration/sync_app_list_helper.h',
          ],
        }],
      ]
    },
    {
      # GN version: //chrome/test:sync_integration_tests
      'target_name': 'sync_integration_tests',
      'type': 'executable',
      'dependencies': [
        'chrome_resources.gyp:chrome_resources',
        'chrome_resources.gyp:chrome_strings',
        'chrome_resources.gyp:packed_extra_resources',
        'chrome_resources.gyp:packed_resources',
        'common',
        'renderer',
        'test_support_common',
        'test_support_sync_integration',
        'test_support_ui',
        '../components/sync.gyp:sync',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        '../third_party/icu/icu.gyp:icui18n',
        '../third_party/icu/icu.gyp:icuuc',
        '../third_party/leveldatabase/leveldatabase.gyp:leveldatabase',
        '../third_party/WebKit/public/blink.gyp:blink',
      ],
      'include_dirs': [
        '..',
        '<(INTERMEDIATE_DIR)',
        '<(protoc_out_dir)',
      ],
      # TODO(phajdan.jr): Only temporary, to make transition easier.
      'defines': [
        'HAS_OUT_OF_PROC_TEST_RUNNER',
      ],
      'sources': [
        '<@(sync_integration_tests_sources)',
      ],
      'conditions': [
        ['OS=="linux"', {
          'dependencies': [
            '../build/linux/system.gyp:nss',
          ],
        }],
        ['OS=="mac"', {
          # The sync_integration_tests do not run on mac without this flag.
          # Search for comments about "xcode_settings" elsewhere in this file.
          'xcode_settings': {'OTHER_LDFLAGS': ['-Wl,-ObjC']},
          # Dictionary sync is disabled on Mac.
          # Note: list duplicated in GN build.
          'sources!': [
            'browser/sync/test/integration/single_client_dictionary_sync_test.cc',
            'browser/sync/test/integration/two_client_dictionary_sync_test.cc',
          ],
        }],
        ['OS=="win"', {
          'sources': [
            '<(SHARED_INTERMEDIATE_DIR)/chrome_version/other_version.rc',
            '<(SHARED_INTERMEDIATE_DIR)/ui/resources/ui_unscaled_resources.rc',
          ],
          'include_dirs': [
            '<(DEPTH)/third_party/wtl/include',
          ],
          'dependencies': [
            'chrome_version_resources',
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
        ['chromeos == 0', {
          'sources!': [
            # Note: this list is duplicated in the GN build.
	    "browser/sync/test/integration/single_client_arc_package_sync_test.cc",
            'browser/sync/test/integration/single_client_wifi_credentials_sync_test.cc',
	    "browser/sync/test/integration/two_client_arc_package_sync_test.cc",
            'browser/sync/test/integration/two_client_wifi_credentials_sync_test.cc',
          ],
        }],
        ['enable_basic_printing==1 or enable_print_preview==1', {
          'dependencies': [
            '../printing/printing.gyp:printing',
          ],
        }],
        ['enable_app_list==0', {
          'sources!': [
            'browser/sync/test/integration/single_client_app_list_sync_test.cc',
            'browser/sync/test/integration/two_client_app_list_sync_test.cc',
          ],
        }],
        ['enable_supervised_users==0', {
          'sources!': [
            'browser/sync/test/integration/single_client_supervised_user_settings_sync_test.cc',
          ],
        }],
      ],
    },
    {
      # GN version: //chrome/test:sync_performance_tests
      'target_name': 'sync_performance_tests',
      'type': 'executable',
      'dependencies': [
        'test_support_sync_integration',
        'test_support_ui',
        '../components/sync.gyp:sync',
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
      ],
      'sources': [
        '<@(sync_performance_tests_sources)',
      ],
      'conditions': [
        ['OS=="linux"', {
          'dependencies': [
            '../build/linux/system.gyp:nss',
          ],
        }],
        ['OS=="mac"', {
          # The sync_performance_tests do not run on mac without this flag.
          # Search for comments about "xcode_settings" elsewhere in this file.
          'xcode_settings': {'OTHER_LDFLAGS': ['-Wl,-ObjC']},
          # Dictionary sync is disabled on Mac.
          'sources!': [
            'browser/sync/test/integration/performance/dictionary_sync_perf_test.cc',
          ],
        }],
        ['OS=="win"', {
          'sources': [
            '<(SHARED_INTERMEDIATE_DIR)/chrome_version/other_version.rc',
          ],
          'include_dirs': [
            '<(DEPTH)/third_party/wtl/include',
          ],
          'dependencies': [
            'chrome_version_resources',
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
      # Executable to measure time to load libraries.
      # GN version: //chrome/test:load_library_perf_tests
      'target_name': 'load_library_perf_tests',
      'type': '<(gtest_target_type)',
      'dependencies': [
        '../base/base.gyp:test_support_perf',
        '../testing/gtest.gyp:gtest',
        '../testing/perf/perf_test.gyp:*',
        '../third_party/widevine/cdm/widevine_cdm.gyp:widevine_cdm_version_h',
      ],
      'sources': [
        'browser/load_library_perf_test.cc',
      ],
      'include_dirs': [
        '..',
        '<(SHARED_INTERMEDIATE_DIR)',
      ],
      'conditions': [
        ['enable_pepper_cdms==1', {
          'dependencies': [
            'test_support_common',
            '../media/media.gyp:cdm_paths',
            # Runtime dependencies.
            '../media/media.gyp:clearkeycdmadapter',
            '../third_party/widevine/cdm/widevine_cdm.gyp:widevinecdmadapter',
          ],
        }],
      ],
    },  # target 'load_library_perf_tests'
  ],
  'conditions': [
    ['OS == "android"', {
        'variables' : {
           'test_support_apk_target' : 'chrome_public_test_support_apk',
           'test_support_apk_name' : 'ChromePublicTestSupport',
           'test_support_apk_manifest_path' : '../chrome/test/android/chrome_public_test_support/AndroidManifest.xml',
            'test_support_apk_dependencies' : ['cast_emulator',],

        },
        'includes' : [
            'chrome_test_support.gypi',
        ],
      'targets': [
        {
          # GN: //chrome/android:chrome_junit_tests
          'target_name': 'chrome_junit_tests',
          'type': 'none',
          'dependencies': [
            # Allow unit-testing Chrome UI components
            'android/chrome_apk.gyp:chrome_public_apk_java',
            'chrome_java',
            '../base/base.gyp:base',
            '../base/base.gyp:base_java_test_support',
            '../base/base.gyp:base_junit_test_support',
            '../components/sync.gyp:sync_java_test_support',
            '../testing/android/junit/junit_test.gyp:junit_test_support',
          ],
          'variables': {
            'main_class': 'org.chromium.testing.local.JunitTestMain',
            'src_paths': [
              'android/junit/',
            ],
            'test_type': 'junit',
            'wrapper_script_name': 'helper/<(_target_name)',
          },
          'includes': [
            '../build/android/test_runner.gypi',
            '../build/host_jar.gypi',
          ],
        },
        {
          # GN: //chrome/test/chromedriver/test/webview_shell:chromedriver_webview_shell_apk
          'target_name': 'chromedriver_webview_shell_apk',
          'type': 'none',
          'variables': {
            'apk_name': 'ChromeDriverWebViewShell',
            'java_in_dir': 'test/chromedriver/test/webview_shell/java',
            'resource_dir': 'test/chromedriver/test/webview_shell/java/res',
          },
          'includes': [ '../build/java_apk.gypi' ],
        },
        {
          # GN: //chrome/test/android:chrome_java_test_support
          'target_name': 'chrome_java_test_support',
          'type': 'none',
          'variables': {
            'java_in_dir': '../chrome/test/android/javatests',
          },
          'dependencies': [
            'chrome_java',
            '../base/base.gyp:base_java',
            '../base/base.gyp:base_java_test_support',
            '../components/components.gyp:policy_java_test_support',
            '../content/content_shell_and_tests.gyp:content_java_test_support',
            '../net/net.gyp:net_java',
            '../net/net.gyp:net_java_test_support',
            '../components/sync.gyp:sync_java',
            '../components/sync.gyp:sync_java_test_support',
            '../third_party/android_tools/android_tools.gyp:google_play_services_javalib'
          ],
          'includes': [ '../build/java.gypi' ],
        },
        {
          # GN: //chrome/test/android/cast_emulator:cast_emulator
          'target_name': 'cast_emulator',
          'type': 'none',
          'dependencies': [
            '../base/base.gyp:base_java',
            '../third_party/android_tools/android_tools.gyp:android_support_v7_appcompat_javalib',
            '../third_party/android_tools/android_tools.gyp:android_support_v7_mediarouter_javalib',
            '../third_party/android_tools/android_tools.gyp:google_play_services_javalib',
          ],
          'variables': {
            'java_in_dir': '../chrome/test/android/cast_emulator',
          },
          'includes': [ '../build/java.gypi' ],
        },
      ],
      'conditions': [
        ['test_isolation_mode != "noop"',
          {
            'targets': [{
               'target_name': 'telemetry_perf_unittests_android_run',
               'type': 'none',
               'dependencies': [
                  '../content/content_shell_and_tests.gyp:telemetry_base',
                  'android/chrome_apk.gyp:chrome_public_apk',
               ],
               'includes': [
                 '../build/isolate.gypi',
                ],
                'sources': [
                  'telemetry_perf_unittests_android.isolate',
                ],
            }],
          }
        ],
      ],
    }],
    ['test_isolation_mode != "noop"', {
      'targets': [
        {
          'target_name': 'telemetry_chrome_test_base',
          'type': 'none',
          'dependencies': [
            '../content/content_shell_and_tests.gyp:telemetry_base',
          ],
          'conditions': [
            ['OS=="linux" or OS=="mac"', {
              'dependencies': [
                '../breakpad/breakpad.gyp:dump_syms#host',
              ],
            }],
            ['OS=="mac" or OS=="win"', {
              'dependencies': [
                '../third_party/crashpad/crashpad/tools/tools.gyp:crashpad_database_util',
              ],
            }],
            ['OS=="win"', {
              'dependencies': [
                'copy_cdb_to_output',
              ],
            }],
          ],
        },
        {
          'target_name': 'gpu_tests_base',
          'type': 'none',
          'dependencies': [
            # depend on icu to fix races. http://crbug.com/417583
            '../third_party/icu/icu.gyp:icudata',
          ],
          # Set this so we aren't included as a target in files that
          # include this file via a wildcard (such as chrome_tests.gypi).
          # If we didn't do this the All target ends up with a rule that
          # makes it unnecessarily compile in certain situations.
          'suppress_wildcard': 1,
          'direct_dependent_settings': {
            'includes': [
              '../build/isolate.gypi',
            ],
          },
        },
        {
          # GN: //gpu:angle_unittests_run
          'target_name': 'angle_unittests_run',
          'type': 'none',
          'dependencies': [
            '../gpu/gpu.gyp:angle_unittests',
            'gpu_tests_base',
          ],
          'sources': [
            'angle_unittests.isolate',
          ],
        },
        {
          'target_name': 'browser_tests_run',
          'type': 'none',
          'dependencies': [
            'browser_tests',
            'chrome',
          ],
          'includes': [
            '../build/isolate.gypi',
          ],
          'sources': [
            'browser_tests.isolate',
          ],
          'conditions': [
            ['use_x11==1', {
              'dependencies': [
                '../tools/xdisplaycheck/xdisplaycheck.gyp:xdisplaycheck',
              ],
            }],
          ],
        },
        {
         'target_name': 'telemetry_perf_unittests_run',
         'type': 'none',
         'dependencies': [
            'chrome_run',
            'telemetry_chrome_test_base'
         ],
         'includes': [
           '../build/isolate.gypi',
          ],
          'sources': [
            'telemetry_perf_unittests.isolate',
          ],
        },
        {
         'target_name': 'telemetry_unittests_run',
         'type': 'none',
         'dependencies': [
            'chrome_run',
            'telemetry_chrome_test_base'
         ],
         'includes': [
           '../build/isolate.gypi',
          ],
          'sources': [
            'telemetry_unittests.isolate',
          ],
        },
        {
         'target_name': 'telemetry_gpu_unittests_run',
         'type': 'none',
         'dependencies': [
            '../content/content_shell_and_tests.gyp:telemetry_base',
         ],
         'includes': [
           '../build/isolate.gypi',
          ],
          'sources': [
            'telemetry_gpu_unittests.isolate',
          ],
        },
        {
          'target_name': 'chromedriver_unittests_run',
          'type': 'none',
          'dependencies': [
            'chromedriver_unittests',
          ],
          'conditions': [
            ['use_x11 == 1', {
              'dependencies': [
                '../tools/xdisplaycheck/xdisplaycheck.gyp:xdisplaycheck',
              ],
            }],
          ],
          'includes': [
            '../build/isolate.gypi',
          ],
          'sources': [
            'chromedriver_unittests.isolate',
          ],
        },
        {
          'target_name': 'interactive_ui_tests_run',
          'type': 'none',
          'dependencies': [
            'interactive_ui_tests',
          ],
          'conditions': [
            ['use_x11 == 1', {
              'dependencies': [
                '../tools/xdisplaycheck/xdisplaycheck.gyp:xdisplaycheck',
              ],
            }],
          ],
          'includes': [
            '../build/isolate.gypi',
          ],
          'sources': [
            'interactive_ui_tests.isolate',
          ],
        },
        {
          'target_name': 'sync_integration_tests_run',
          'type': 'none',
          'dependencies': [
            'sync_integration_tests',
          ],
          'conditions': [
            ['use_x11 == 1', {
              'dependencies': [
                '../tools/xdisplaycheck/xdisplaycheck.gyp:xdisplaycheck',
              ],
            }],
          ],
          'includes': [
            '../build/isolate.gypi',
          ],
          'sources': [
            'sync_integration_tests.isolate',
          ],
        },
      ],
      'conditions': [
        ['OS=="win"', {
          'targets': [
            {
              'target_name': 'copy_cdb_to_output',
              'type': 'none',
              'actions': [
                {
                  'action_name': 'copy_cdb',
                  'inputs': [
                    '<(DEPTH)/build/win/copy_cdb_to_output.py',
                  ],
                  'outputs': [
                    '<(PRODUCT_DIR)/cdb/cdb.exe',
                    '<(PRODUCT_DIR)/cdb/dbgeng.dll',
                    '<(PRODUCT_DIR)/cdb/dbghelp.dll',
                    '<(PRODUCT_DIR)/cdb/dbgmodel.dll',
                    '<(PRODUCT_DIR)/cdb/winext/ext.dll',
                    '<(PRODUCT_DIR)/cdb/winext/uext.dll',
                    '<(PRODUCT_DIR)/cdb/winxp/exts.dll',
                    '<(PRODUCT_DIR)/cdb/winxp/ntsdexts.dll',
                  ],
                  'action': ['python',
                             '<(DEPTH)/build/win/copy_cdb_to_output.py',
                             '<(PRODUCT_DIR)/cdb',
                             '<(target_arch)'],
                  'message': 'Copying cdb and deps to <(PRODUCT_DIR)/cdb',
                },
              ],
            },
          ],
        }],
        ['archive_gpu_tests==1', {
          'targets': [
            {
              # GN: //gpu:gl_tests_run
              'target_name': 'gl_tests_run',
              'type': 'none',
              'dependencies': [
                '../gpu/gpu.gyp:gl_tests',
                'gpu_tests_base',
              ],
              'sources': [
                'gl_tests.isolate',
              ],
            },
            {
              # GN: //chrome/test:tab_capture_end2end_tests_run
              'target_name': 'tab_capture_end2end_tests_run',
              'type': 'none',
              'dependencies': [
                'browser_tests_run',
                'gpu_tests_base',
              ],
              'sources': [
                'tab_capture_end2end_tests.isolate',
              ],
            },
            {
              'target_name': 'telemetry_gpu_integration_test_run',
              'type': 'none',
              'dependencies': [
                'chrome_run',
                'gpu_tests_base',
                'telemetry_chrome_test_base',
              ],
              'sources': [
                'telemetry_gpu_integration_test.isolate',
              ],
            },
            {
              'target_name': 'telemetry_gpu_test_run',
              'type': 'none',
              'dependencies': [
                'chrome_run',
                'gpu_tests_base',
                'telemetry_chrome_test_base',
              ],
              'sources': [
                'telemetry_gpu_test.isolate',
              ],
            },
          ],
          'conditions': [
            ['internal_gles2_conform_tests==1', {
              'targets': [
                {
                  # GN: //gpu/gles2_conform_support:gles2_conform_test_run
                  'target_name': 'gles2_conform_test_run',
                  'type': 'none',
                  'dependencies': [
                    '../gpu/gles2_conform_support/gles2_conform_test.gyp:gles2_conform_test',
                    'gpu_tests_base',
                  ],
                  'sources': [
                    'gles2_conform_test.isolate',
                  ],
                },
              ],
            }],
            ['OS=="win" or OS=="linux" or OS=="mac"', {
              'targets': [
                {
                  'target_name': 'angle_end2end_tests_run',
                  'type': 'none',
                  'dependencies': [
                    '../gpu/gpu.gyp:angle_end2end_tests',
                    'gpu_tests_base',
                  ],
                  'sources': [
                    'angle_end2end_tests.isolate',
                  ],
                },
              ],
            }],
            ['build_angle_deqp_tests==1', {
              'targets': [
                {
                  'target_name': 'angle_deqp_gles2_tests_run',
                  'type': 'none',
                  'dependencies': [
                    '../gpu/gpu.gyp:angle_deqp_gles2_tests',
                    'gpu_tests_base',
                  ],
                  'sources': [
                    'angle_deqp_gles2_tests.isolate',
                  ],
                },
                {
                  'target_name': 'angle_deqp_gles3_tests_run',
                  'type': 'none',
                  'dependencies': [
                    '../gpu/gpu.gyp:angle_deqp_gles3_tests',
                    'gpu_tests_base',
                  ],
                  'sources': [
                    'angle_deqp_gles3_tests.isolate',
                  ],
                },
                {
                  'target_name': 'angle_deqp_egl_tests_run',
                  'type': 'none',
                  'dependencies': [
                    '../gpu/gpu.gyp:angle_deqp_egl_tests',
                    'gpu_tests_base',
                  ],
                  'sources': [
                    'angle_deqp_egl_tests.isolate',
                  ],
                },
              ],
            }],
          ],
        }],
      ],
    }],
    [ 'enable_mdns == 1', {
      'targets': [{
          # GN version: //chrome/tools/service_discovery_sniffer
          'target_name': 'service_discovery_sniffer',
          'type': 'executable',
          'dependencies': [
            '../net/net.gyp:net',
            '../base/base.gyp:base',
            '../base/base.gyp:test_support_base',
            # TODO(vitalybuka): Extract mdns code into lib or component.
            'browser',
          ],
          'sources': [
            'tools/service_discovery_sniffer/service_discovery_sniffer.cc',
            'tools/service_discovery_sniffer/service_discovery_sniffer.h',
          ],
        }]
    }],
  ],  # 'conditions'
}
