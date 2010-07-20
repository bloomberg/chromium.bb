# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
    'xul_sdk_dir': '../third_party/xulrunner-sdk/<(OS)',

    'variables': {
      'version_py_path': '../tools/build/version.py',
      'version_path': 'VERSION',
    },
    'version_py_path': '<(version_py_path) -f',
    'version_path': '<(version_path)',

    # Keep the archive builder happy.
    'chrome_personalization%': 1,
    'use_syncapi_stub%': 0,

    # Deps info.
    'xul_include_directories': [
      '<(xul_sdk_dir)/include/caps',
      '<(xul_sdk_dir)/include/dom',
      '<(xul_sdk_dir)/include/js',
      '<(xul_sdk_dir)/include/nspr',
      '<(xul_sdk_dir)/include/string',
      '<(xul_sdk_dir)/include/xpcom',
      '<(xul_sdk_dir)/include/xpconnect',
    ],  
    'conditions': [
      ['OS=="win"', {
        'python': [
          '<(DEPTH)\\third_party\\python_24\\setup_env.bat && python'
        ],
      }, { # OS != win
        'python': [
          'python'
        ],
      }],
    ],
  },
  'includes': [
    '../build/common.gypi',
  ],
  'target_defaults': {
    'dependencies': [
      '../chrome/chrome.gyp:chrome_resources',
      '../chrome/chrome.gyp:chrome_strings',
      '../chrome/chrome.gyp:theme_resources',
      '../skia/skia.gyp:skia',
      '../third_party/npapi/npapi.gyp:npapi',
    ],
    'include_dirs': [
      # all our own includes are relative to src/
      '..',
    ],
  },
  'targets': [
    {
      # TODO(slightlyoff): de-win23-ify
      'target_name': 'xulrunner_sdk',
      'type': 'none',
      'conditions': [
        ['OS=="win"', {
          'direct_dependent_settings': {
            'include_dirs': [
              '<@(xul_include_directories)',
            ],
            'libraries': [
              '../third_party/xulrunner-sdk/win/lib/xpcomglue_s.lib',
              '../third_party/xulrunner-sdk/win/lib/xpcom.lib',
              '../third_party/xulrunner-sdk/win/lib/nspr4.lib',
            ],
          },
        },],
      ],
    },
    {
      # TODO(slightlyoff): de-win32-ify
      #
      # build the base_noicu.lib.
      'target_name': 'base_noicu',
      'type': 'none',
      'dependencies': [
        '../base/base.gyp:base',
      ],
      'actions': [
        {
          'action_name': 'combine_libs',
          'msvs_cygwin_shell': 0,
          'inputs': [
            '<(PRODUCT_DIR)/lib/base.lib',
          ],
          'outputs': [
            '<(PRODUCT_DIR)/lib/base_noicu.lib',
          ],
          'action': [
            '<@(python)',
            'combine_libs.py',
            '-o', '<@(_outputs)',
            '-r (icu_|_icu.obj)',
            '<@(_inputs)'],
        },
      ],
      'direct_dependent_settings': {
        # linker_settings
        'libraries': [
          '<(PRODUCT_DIR)/lib/base_noicu.lib',
        ],
      },
    },
    {
      'target_name': 'chrome_frame_unittests',
      'type': 'executable',
      'msvs_guid': '17D98CCA-0F6A-470F-9DF9-56DC6CC1A0BE',
      'dependencies': [
        '../build/temp_gyp/googleurl.gyp:googleurl',
        '../chrome/chrome.gyp:browser',     
        '../chrome/chrome.gyp:common',
        '../chrome/chrome.gyp:debugger',
        '../chrome/chrome.gyp:nacl',
        '../chrome/chrome.gyp:renderer',
        '../chrome/chrome.gyp:utility',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        'base_noicu',
        'chrome_frame_ie',
        'chrome_frame_npapi',
        'chrome_frame_strings',
        'xulrunner_sdk',
      ],
      'sources': [
        'chrome_tab.h',
        'chrome_tab.idl',
        'chrome_frame_histograms.h',
        'chrome_frame_histograms.cc',
        'chrome_frame_npapi_unittest.cc',
        'chrome_frame_unittest_main.cc',
        'chrome_launcher.cc',
        'chrome_launcher.h',
        'chrome_launcher_unittest.cc',
        'function_stub_unittest.cc',
        'test/chrome_frame_test_utils.h',
        'test/chrome_frame_test_utils.cc',
        'test/com_message_event_unittest.cc',
        'test/exception_barrier_unittest.cc',
        'test/html_util_unittests.cc',
        'test/http_negotiate_unittest.cc',
        'test/simulate_input.h',
        'test/simulate_input.cc',
        'test/urlmon_moniker_tests.h',
        'test/urlmon_moniker_unittest.cc',
        'test/util_unittests.cc',
        'test/window_watchdog.h',
        'test/window_watchdog.cc',
        'unittest_precompile.h',
        'unittest_precompile.cc',
        'urlmon_upload_data_stream.cc',
        'urlmon_upload_data_stream_unittest.cc',
        'vtable_patch_manager_unittest.cc',
      ],
      'include_dirs': [
        # To allow including "chrome_tab.h"
        '<(INTERMEDIATE_DIR)',
      ],
      'resource_include_dirs': [
        '<(INTERMEDIATE_DIR)',
        '<(SHARED_INTERMEDIATE_DIR)',
      ],
      'conditions': [
        ['OS=="win"', {
          'link_settings': {
            'libraries': [
              '-lshdocvw.lib',
            ],
          },
          'msvs_settings': {
            'VCLinkerTool': {
              'DelayLoadDLLs': ['xpcom.dll', 'nspr4.dll', 'shdocvw.dll'],
            },
          },
          'dependencies': [
            # TODO(slightlyoff): Get automation targets working on OS X
            '../chrome/chrome.gyp:automation',
            '../chrome/chrome.gyp:installer_util',
            '../google_update/google_update.gyp:google_update',
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
      'target_name': 'chrome_frame_tests',
      'msvs_guid': '1D25715A-C8CE-4448-AFA3-8515AF22D235',
      'type': 'executable',
      'dependencies': [
        '../build/temp_gyp/googleurl.gyp:googleurl',
        '../chrome/chrome.gyp:common',
        '../chrome/chrome.gyp:utility',
        '../net/net.gyp:net_test_support',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        '../third_party/libxslt/libxslt.gyp:libxslt',
        'chrome_frame_ie',
        'chrome_frame_npapi',
        'chrome_frame_strings',
        'npchrome_frame',
        'xulrunner_sdk',
      ],
      'sources': [
        '../base/test_suite.h',
        'bind_context_info.cc',
        'bind_context_info.h',
        'test/automation_client_mock.cc',
        'test/automation_client_mock.h',
        'test/chrome_frame_test_utils.cc',
        'test/chrome_frame_test_utils.h',
        'test/chrome_frame_automation_mock.cc',
        'test/chrome_frame_automation_mock.h',
        'test/http_server.cc',
        'test/http_server.h',
        'test/ie_event_sink.cc',
        'test/ie_event_sink.h',
        'test/mock_ie_event_sink_actions.h',
        'test/mock_ie_event_sink_test.cc',
        'test/mock_ie_event_sink_test.h',
        'test/navigation_test.cc',
        'test/proxy_factory_mock.cc',
        'test/proxy_factory_mock.h',
        'test/run_all_unittests.cc',
        'test/simulate_input.cc',
        'test/simulate_input.h',
        'test/test_server.cc',
        'test/test_server.h',
        'test/test_server_test.cc',
        'test/test_with_web_server.cc',
        'test/test_with_web_server.h',
        'test/ui_test.cc',
        'test/urlmon_moniker_tests.h',
        'test/urlmon_moniker_integration_test.cc',
        'test/url_request_test.cc',
        'test/window_watchdog.cc',
        'test/window_watchdog.h',
        'chrome_frame_automation.cc',
        'chrome_frame_histograms.h',
        'chrome_frame_histograms.cc',
        'chrome_tab.h',
        'chrome_tab.idl',
        'com_message_event.cc',
        'custom_sync_call_context.h',        
        'html_utils.cc',
        'http_negotiate.h',
        'http_negotiate.cc',
        'sync_msg_reply_dispatcher.cc',
        'sync_msg_reply_dispatcher.h',
        'test_utils.cc',
        'test_utils.h',
        'urlmon_upload_data_stream.h',
        'urlmon_upload_data_stream.cc',
        'urlmon_url_request.h',
        'urlmon_url_request.cc',
        'utils.cc',
        'utils.h',
        'vtable_patch_manager.h',
        'vtable_patch_manager.cc'
      ],
      'include_dirs': [
        '<@(xul_include_directories)',
        '<(DEPTH)/third_party/wtl/include',
        # To allow including "chrome_tab.h"
        '<(INTERMEDIATE_DIR)',
      ],
      'resource_include_dirs': [
        '<(INTERMEDIATE_DIR)',
      ],
      'conditions': [
        ['OS=="win"', {
          'msvs_settings': {
            'VCLinkerTool': {
              'DelayLoadDLLs': ['xpcom.dll', 'nspr4.dll'],
            },
          },
          'dependencies': [
            '../chrome/chrome.gyp:automation',
            '../chrome/chrome.gyp:installer_util',
            '../google_update/google_update.gyp:google_update',
          ]
        }],
      ],
    },
    {
      'target_name': 'chrome_frame_perftests',
      'msvs_guid': '3767888B-76ED-4D2A-B1F5-263CC56A12AA',
      'type': 'executable',
      'dependencies': [
        '../base/base.gyp:base_i18n',
        '../base/base.gyp:test_support_base',
        '../build/temp_gyp/googleurl.gyp:googleurl',
        '../chrome/chrome.gyp:common',
        '../chrome/chrome.gyp:utility',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        '../third_party/libxml/libxml.gyp:libxml',
        '../third_party/libxslt/libxslt.gyp:libxslt',
        'chrome_frame_ie',
        'chrome_frame_npapi',
        'chrome_frame_strings',
        'chrome_frame_utils',
        'npchrome_frame',
        'xulrunner_sdk',
      ],
      'sources': [
        '../base/perf_test_suite.h',
        '../base/perftimer.cc',
        '../base/test/test_file_util.h',
        '../chrome/test/chrome_process_util.cc',
        '../chrome/test/chrome_process_util.h',
        '../chrome/test/ui/ui_test.cc',
        'chrome_tab.h',
        'chrome_tab.idl',
        'html_utils.cc',
        'test/chrome_frame_test_utils.cc',
        'test/chrome_frame_test_utils.h',
        'test/perf/chrome_frame_perftest.cc',
        'test/perf/chrome_frame_perftest.h',
        'test/perf/run_all.cc',
        'test/perf/silverlight.cc',
        'test/simulate_input.cc',
        'test/simulate_input.h',
        'test_utils.cc',
        'test_utils.h',
        'test/window_watchdog.cc',
        'test/window_watchdog.h',
        'utils.cc',
        'utils.h',
      ],
      'include_dirs': [
        '<@(xul_include_directories)',
        '<(DEPTH)/third_party/wtl/include',
        # To allow including "chrome_tab.h"
        '<(INTERMEDIATE_DIR)',
      ],
      'conditions': [
        ['OS=="win"', {
          'dependencies': [
            '../breakpad/breakpad.gyp:breakpad_handler',
            '../chrome/chrome.gyp:automation',
            '../chrome/chrome.gyp:installer_util',
            '../google_update/google_update.gyp:google_update',
          ],
          'sources': [
            '../base/test/test_file_util_win.cc',
          ]
        }],
      ],
    },

    {
      'target_name': 'chrome_frame_net_tests',
      'msvs_guid': '8FDA8275-0415-4B08-A1DC-C95B0D3708DB',
      'type': 'executable',
      'dependencies': [
        '../base/base.gyp:test_support_base',
        '../chrome/chrome.gyp:browser',
        '../chrome/chrome.gyp:chrome_resources',
        '../chrome/chrome.gyp:debugger',
        '../chrome/chrome.gyp:renderer',
        '../chrome/chrome.gyp:syncapi',
        '../net/net.gyp:net_test_support',
        '../skia/skia.gyp:skia',
        '../testing/gtest.gyp:gtest',
        '../third_party/icu/icu.gyp:icui18n',
        '../third_party/icu/icu.gyp:icuuc',
        'chrome_frame_npapi',
        'chrome_frame_ie',
        'npchrome_frame',
      ],
      'sources': [
        '../net/url_request/url_request_unittest.cc',
        '../net/url_request/url_request_unittest.h',
        'test/chrome_frame_test_utils.cc',
        'test/chrome_frame_test_utils.h',
        'test/simulate_input.cc',
        'test/simulate_input.h',
        'test/test_server.cc',
        'test/test_server.h',
        'test/window_watchdog.cc',
        'test/window_watchdog.h',
        'test/net/fake_external_tab.cc',
        'test/net/fake_external_tab.h',
        'test/net/process_singleton_subclass.cc',
        'test/net/process_singleton_subclass.h',
        'test/net/test_automation_provider.cc',
        'test/net/test_automation_provider.h',
        'test/net/test_automation_resource_message_filter.cc',
        'test/net/test_automation_resource_message_filter.h',
        'chrome_tab.h',
        'chrome_tab.idl',
      ],
      'include_dirs': [
        # To allow including "chrome_tab.h"
        '<(INTERMEDIATE_DIR)',
      ],
      'conditions': [
        ['OS=="win"', {
          'msvs_settings': {
            'VCLinkerTool': {
              'DelayLoadDLLs': ['prntvpt.dll'],
            },
          },
          'dependencies': [
            '../breakpad/breakpad.gyp:breakpad_handler',
            '../chrome/chrome.gyp:automation',
            '../chrome/chrome.gyp:chrome_dll_version',
            '../chrome/chrome.gyp:installer_util',
            '../google_update/google_update.gyp:google_update',
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
      'target_name': 'chrome_frame_reliability_tests',
      'type': 'executable',
      'msvs_guid': 'A1440368-4089-4E14-8864-D84D3C5714A7',
      'dependencies': [
        '../chrome/chrome.gyp:browser',
        '../chrome/chrome.gyp:renderer',
        '../testing/gtest.gyp:gtest',
        'base_noicu',
        'chrome_frame_ie',
        'chrome_frame_npapi',
        'chrome_frame_strings',
      ],
      'sources': [
        'test/reliability/run_all_unittests.cc',
        'test/reliability/page_load_test.cc',
        'test/reliability/page_load_test.h',
        'test/reliability/reliability_test_suite.h',
        'test/chrome_frame_test_utils.cc',
        'test/chrome_frame_test_utils.h',
        'test_utils.cc',
        'test_utils.h',
        'test/simulate_input.cc',
        'test/simulate_input.h',
        'test/window_watchdog.cc',
        'test/window_watchdog.h',
        'chrome_tab.h',
        'chrome_tab.idl',
        '../base/test/test_file_util_win.cc',
        '../chrome/test/ui/ui_test.cc',
        '../chrome/test/ui/ui_test_suite.cc',
        '../chrome/test/ui/ui_test_suite.h',
        '../chrome/test/chrome_process_util.cc',
        '../chrome/test/chrome_process_util.h',
      ],
      'include_dirs': [
        # To allow including "chrome_tab.h"
        '<(INTERMEDIATE_DIR)',
      ],
      'resource_include_dirs': [
        '<(INTERMEDIATE_DIR)',
      ],
      'conditions': [
        ['OS=="win"', {
          'dependencies': [
            # TODO(slightlyoff): Get automation targets working on OS X
            '../chrome/chrome.gyp:automation',
            '../chrome/chrome.gyp:installer_util',
            '../google_update/google_update.gyp:google_update',
          ]
        }],
      ],
    },

    {
      'target_name': 'chrome_frame_npapi',
      'type': 'static_library',
      'dependencies': [
        'chrome_frame_strings',
        'chrome_frame_utils',
        '../chrome/chrome.gyp:common',
        'xulrunner_sdk',
      ],
      'sources': [
        'chrome_frame_automation.cc',
        'chrome_frame_automation.h',
        'chrome_frame_delegate.cc',
        'chrome_frame_delegate.h',
        'chrome_frame_plugin.h',
        'chrome_frame_npapi.cc',
        'chrome_frame_npapi.h',
        'custom_sync_call_context.h',
        'ff_30_privilege_check.cc',
        'ff_privilege_check.h',
        'html_utils.cc',
        'html_utils.h',
        'np_browser_functions.cc',
        'np_browser_functions.h',
        'np_event_listener.cc',
        'np_event_listener.h',
        'np_proxy_service.cc',
        'np_proxy_service.h',
        'np_utils.cc',
        'np_utils.h',
        'npapi_url_request.cc',
        'npapi_url_request.h',
        'ns_associate_iid_win.h',
        'ns_isupports_impl.h',
        'plugin_url_request.cc',
        'plugin_url_request.h',
        'scoped_ns_ptr_win.h',
        'sync_msg_reply_dispatcher.cc',
        'sync_msg_reply_dispatcher.h',
        'utils.cc',
        'utils.h',
      ],
    },
    {
      'target_name': 'chrome_frame_strings',
      'type': 'none',
      'rules': [
        {
          'rule_name': 'grit',
          'extension': 'grd',
          'inputs': [
            '../tools/grit/grit.py',
          ],
          'variables': {
            'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/chrome_frame',
          },
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/chrome_frame/grit/<(RULE_INPUT_ROOT).h',
            '<(SHARED_INTERMEDIATE_DIR)/chrome_frame/<(RULE_INPUT_ROOT).pak',
          ],
          'action': ['python', '<@(_inputs)', '-i', 
            '<(RULE_INPUT_PATH)',
            'build', '-o', '<(grit_out_dir)'
          ],
          'message': 'Generating resources from <(RULE_INPUT_PATH)',
        },
      ],
      'sources': [
        # Localizable resources.
        'resources/chrome_frame_resources.grd',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(SHARED_INTERMEDIATE_DIR)/chrome_frame',
        ],
      },
      'conditions': [
        ['OS=="win"', {
          'dependencies': ['../build/win/system.gyp:cygwin'],
        }],
      ],
    },
    {
      'target_name': 'chrome_frame_utils',
       # The intent is that shared util code can be built into a separate lib.
       # Currently on the resource loading code is here.
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base_i18n',
       ],
      'sources': [
        'simple_resource_loader.cc',
        'simple_resource_loader.h',
      ],
    },
    {
      'target_name': 'chrome_frame_ie',
      'type': 'static_library',
      'dependencies': [
        'chrome_frame_strings',
        'chrome_frame_utils',
        '../chrome/chrome.gyp:common',
        '../chrome/chrome.gyp:utility',
        '../build/temp_gyp/googleurl.gyp:googleurl',
        '../third_party/libxml/libxml.gyp:libxml',
        '../third_party/bzip2/bzip2.gyp:bzip2',
      ],
      'sources': [
        'bho.cc',
        'bho.h',
        'bho.rgs',
        'bind_context_info.cc',
        'bind_context_info.h',
        'bind_status_callback_impl.cc',
        'bind_status_callback_impl.h',
        'chrome_active_document.cc',
        'chrome_active_document.h',
        'chrome_active_document.rgs',
        'chrome_frame_activex.cc',
        'chrome_frame_activex.h',
        'chrome_frame_activex.rgs',
        'chrome_frame_activex_base.h',
        'chrome_frame_histograms.cc',
        'chrome_frame_histograms.h',
        'chrome_frame_reporting.cc',
        'chrome_frame_reporting.h',
        'chrome_launcher_utils.cc',
        'chrome_launcher_utils.h',
        'chrome_protocol.cc',
        'chrome_protocol.h',
        'chrome_protocol.rgs',
        'chrome_tab.h',
        'chrome_tab.idl',
        'com_message_event.cc',
        'com_message_event.h',
        'com_type_info_holder.cc',
        'com_type_info_holder.h',
        'delete_chrome_history.cc',
        'delete_chrome_history.h',
        'exception_barrier.cc',
        'exception_barrier.h',
        'exception_barrier_lowlevel.asm',
        'find_dialog.cc',
        'find_dialog.h',
        'function_stub.h',
        'http_negotiate.cc',
        'function_stub.cc',
        'http_negotiate.h',
        'iids.cc',
        'in_place_menu.h',
        'metrics_service.cc',
        'metrics_service.h',
        'module_utils.cc',
        'module_utils.h',
        'ole_document_impl.h',
        'protocol_sink_wrap.cc',
        'protocol_sink_wrap.h',
        'register_bho.rgs',
        'stream_impl.cc',
        'stream_impl.h',
        'sync_msg_reply_dispatcher.cc',
        'sync_msg_reply_dispatcher.h',
        'extra_system_apis.h',
        'urlmon_bind_status_callback.h',
        'urlmon_bind_status_callback.cc',
        'urlmon_moniker.h',
        'urlmon_moniker.cc',
        'urlmon_url_request.cc',
        'urlmon_url_request.h',
        'urlmon_url_request_private.h',
        'urlmon_upload_data_stream.cc',
        'urlmon_upload_data_stream.h',
        'vtable_patch_manager.cc',
        'vtable_patch_manager.h',
      ],
      'include_dirs': [
        # To allow including "chrome_tab.h"
        '<(INTERMEDIATE_DIR)',
        '<(INTERMEDIATE_DIR)/../chrome_frame',
      ],
      'conditions': [
        ['OS=="win"', {
          # NOTE(slightlyoff):
          #   this is a fix for the include dirs length limit on the resource
          #   compiler, tickled by the xul_include_dirs variable
          'resource_include_dirs': [
            '<(INTERMEDIATE_DIR)'
          ],
          'dependencies': [
            '../breakpad/breakpad.gyp:breakpad_handler',
            '../chrome/chrome.gyp:automation',
            # Make the archive build happy.
            '../chrome/chrome.gyp:syncapi',
            # Installer
            '../chrome/chrome.gyp:installer_util',
            '../google_update/google_update.gyp:google_update',
            # Crash Reporting
            'crash_reporting/crash_reporting.gyp:crash_report',
            'crash_reporting/crash_reporting.gyp:minidump_test',
            'crash_reporting/crash_reporting.gyp:vectored_handler_tests',
          ],
        },],
      ],
      'rules': [
        {
          'rule_name': 'Assemble',
          'extension': 'asm',
          'inputs': [],
          'outputs': [
            '<(INTERMEDIATE_DIR)/<(RULE_INPUT_ROOT).obj',
          ],
          'action': [
            'ml',
            '/safeseh',
            '/Fo', '<(INTERMEDIATE_DIR)\<(RULE_INPUT_ROOT).obj',
            '/c', '<(RULE_INPUT_PATH)',
          ],
          'process_outputs_as_sources': 0,
          'message': 'Assembling <(RULE_INPUT_PATH) to <(INTERMEDIATE_DIR)\<(RULE_INPUT_ROOT).obj.',
        },
      ],
      'msvs_settings': {
        'VCLinkerTool': {
          'AdditionalOptions': [
            '/safeseh',
          ],
        },
      },
    },
    {
      'target_name': 'npchrome_frame',
      'type': 'shared_library',
      'msvs_guid': 'E3DE7E63-D3B6-4A9F-BCC4-5C8169E9C9F2',
      'dependencies': [
        'base_noicu',
        'chrome_frame_ie',
        'chrome_frame_npapi',
        'chrome_frame_strings',
        'chrome_frame_utils',
        'xulrunner_sdk',
        'chrome_frame_launcher.gyp:chrome_launcher',
        '../chrome/chrome.gyp:chrome_version_info',
        '../chrome/chrome.gyp:chrome_version_header',
        '../chrome/chrome.gyp:common',
        '../chrome/chrome.gyp:utility',
        '../build/temp_gyp/googleurl.gyp:googleurl',
      ],
      'sources': [
        'chrome_frame_npapi.rgs',
        'chrome_frame_npapi_entrypoints.cc',
        'chrome_frame_npapi_entrypoints.h',
        'chrome_tab.cc',
        'chrome_tab.def',
        'chrome_tab.h',
        'chrome_tab.idl',
        # FIXME(slightlyoff): For chrome_tab.tlb. Giant hack until we can
        #   figure out something more gyp-ish.
        'resources/tlb_resource.rc',
        'chrome_tab.rgs',
        'chrome_tab_version.rc',
        'resource.h',
      ],
      'include_dirs': [
        # To allow including "chrome_tab.h"
        '<(INTERMEDIATE_DIR)',
        '<(INTERMEDIATE_DIR)/../chrome_frame',
      ],
      'conditions': [
        ['OS=="win"', {
          # NOTE(slightlyoff):
          #   this is a fix for the include dirs length limit on the resource
          #   compiler, tickled by the xul_include_dirs variable
          'resource_include_dirs': [
            '<(INTERMEDIATE_DIR)'
          ],
          'sources': [
            '<(SHARED_INTERMEDIATE_DIR)/chrome_frame/chrome_frame_resources.rc',
          ],
          'dependencies': [
            '../breakpad/breakpad.gyp:breakpad_handler',
            '../chrome/chrome.gyp:automation',
            # Make the archive build happy.
            '../chrome/chrome.gyp:syncapi',
            # Installer
            '../chrome/chrome.gyp:installer_util',
            '../google_update/google_update.gyp:google_update',
            # Crash Reporting
            'crash_reporting/crash_reporting.gyp:crash_report',
            'crash_reporting/crash_reporting.gyp:vectored_handler_tests',
          ],
          'link_settings': {
            'libraries': [
              '-lshdocvw.lib',
            ],
          },
          'msvs_settings': {
            'VCLinkerTool': {
              'OutputFile':
                  '$(OutDir)\\servers\\$(ProjectName).dll',
              'DelayLoadDLLs': ['xpcom.dll', 'nspr4.dll'],
              'BaseAddress': '0x33000000',
              # Set /SUBSYSTEM:WINDOWS (for consistency).
              'SubSystem': '2',
            },
          },
        }],
      ],
    },
  ],
}

# vim: shiftwidth=2:et:ai:tabstop=2

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
