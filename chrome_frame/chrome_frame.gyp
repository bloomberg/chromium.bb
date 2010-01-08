# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
    'xul_sdk_dir': '../third_party/xulrunner-sdk/<(OS)',

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
            '-o <(_outputs)',
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
        'chrome_frame_npapi_unittest.cc',
        'chrome_frame_unittest_main.cc',
        'chrome_launcher_unittest.cc',
        'test/com_message_event_unittest.cc',
        'test/html_util_unittests.cc',
        'test/http_negotiate_unittest.cc',
        'test/util_unittests.cc',
        'unittest_precompile.h',
        'unittest_precompile.cc',
        'urlmon_upload_data_stream.cc',
        'urlmon_upload_data_stream_unittest.cc',
        'chrome_frame_histograms.h',
        'chrome_frame_histograms.cc',
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
          'msvs_settings': {
            'VCLinkerTool': {
              'DelayLoadDLLs': ['xpcom.dll', 'nspr4.dll'],
            },
          },
          'dependencies': [
            # TODO(slightlyoff): Get automation targets working on OS X
            '../chrome/chrome.gyp:automation',
            '../chrome/installer/installer.gyp:installer_util',
            '../google_update/google_update.gyp:google_update',
          ]
        }],
      ],
    },
    {
      'target_name': 'chrome_frame_tests',
      'type': 'executable',
      'dependencies': [
        '../build/temp_gyp/googleurl.gyp:googleurl',
        '../chrome/chrome.gyp:common',
        '../chrome/chrome.gyp:utility',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        '../third_party/libxml/libxml.gyp:libxml',
        '../third_party/libxslt/libxslt.gyp:libxslt',
        'chrome_frame_strings',
        'chrome_frame_npapi',
        'npchrome_frame',
        'xulrunner_sdk',
      ],
      'sources': [
        '../base/test_suite.h',
        'test/chrome_frame_test_utils.cc',
        'test/chrome_frame_test_utils.h',
        'test/chrome_frame_automation_mock.cc',
        'test/chrome_frame_automation_mock.h',
        'test/chrome_frame_unittests.cc',
        'test/chrome_frame_unittests.h',
        'test/function_stub_unittest.cc',
        'test/http_server.cc',
        'test/http_server.h',
        'test/run_all_unittests.cc',
        'test/test_server.cc',
        'test/test_server.h',
        'test/test_server_test.cc',
        'chrome_frame_automation.cc',
        'chrome_frame_histograms.h',
        'chrome_frame_histograms.cc',
        'chrome_tab.h',
        'chrome_tab.idl',
        'com_message_event.cc',
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
        '../chrome/third_party/wtl/include',
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
            '../chrome/installer/installer.gyp:installer_util',
            '../google_update/google_update.gyp:google_update',
          ]
        }],
      ],
    },
    {
      'target_name': 'chrome_frame_perftests',
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
        'chrome_frame_strings',
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
        'test/perf/chrome_frame_perftest.cc',
        'test/perf/chrome_frame_perftest.h',
        'test/perf/run_all.cc',
        'test/perf/silverlight.cc',
        'test_utils.cc',
        'test_utils.h',
        'utils.cc',
        'utils.h',
      ],
      'include_dirs': [
        '<@(xul_include_directories)',
        '../chrome/third_party/wtl/include',
        # To allow including "chrome_tab.h"
        '<(INTERMEDIATE_DIR)',
      ],
      'conditions': [
        ['OS=="win"', {
          'dependencies': [
            '../chrome/chrome.gyp:automation',
            '../breakpad/breakpad.gyp:breakpad_handler',
            '../chrome/installer/installer.gyp:installer_util',
            '../google_update/google_update.gyp:google_update',
            '../chrome/installer/installer.gyp:installer_util',
          ],
          'sources': [
            '../chrome/test/chrome_process_util_win.cc',
            '../base/test/test_file_util_win.cc',
          ]
        }],
      ],
    },

    {
      'target_name': 'chrome_frame_net_tests',
      'type': 'executable',
      'dependencies': [
        '../base/base.gyp:test_support_base',
        '../chrome/chrome.gyp:browser',
        '../chrome/chrome.gyp:chrome_dll_version',
        '../chrome/chrome.gyp:chrome_resources',
        '../chrome/chrome.gyp:debugger',
        '../chrome/chrome.gyp:renderer',
        '../chrome/chrome.gyp:syncapi',
        '../skia/skia.gyp:skia',
        '../testing/gtest.gyp:gtest',
        '../third_party/icu/icu.gyp:icui18n',
        '../third_party/icu/icu.gyp:icuuc',
        'chrome_frame_npapi',
        'npchrome_frame',
      ],
      'sources': [
        '../net/url_request/url_request_unittest.cc',
        '../net/url_request/url_request_unittest.h',
        'test/chrome_frame_test_utils.cc',
        'test/chrome_frame_test_utils.h',
        'test/test_server.cc',
        'test/test_server.h',
        'test/net/dialog_watchdog.cc',
        'test/net/dialog_watchdog.h',
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
          'dependencies': [
            '../chrome/chrome.gyp:automation',
            '../breakpad/breakpad.gyp:breakpad_handler',
            '../chrome/installer/installer.gyp:installer_util',
            '../google_update/google_update.gyp:google_update',
            '../chrome/installer/installer.gyp:installer_util',
          ]
        }],
      ],
    },

    {
      'target_name': 'chrome_frame_reliability_tests',
      'type': 'executable',
      'dependencies': [
        '../build/temp_gyp/googleurl.gyp:googleurl',
        '../chrome/chrome.gyp:browser',
        '../chrome/chrome.gyp:utility',
        '../testing/gtest.gyp:gtest',
        '../base/allocator/allocator.gyp:allocator',
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
        'chrome_tab.h',
        'chrome_tab.idl',
        '../base/test/test_file_util_win.cc',
        '../chrome/test/ui/ui_test.cc',
        '../chrome/test/ui/ui_test_suite.cc',
        '../chrome/test/ui/ui_test_suite.h',
        '../chrome/test/chrome_process_util_win.cc',
        '../chrome/test/chrome_process_util.cc',
        '../chrome/test/chrome_process_util.h',
        '../chrome/test/chrome_process_util_win.cc',
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
            '../chrome/installer/installer.gyp:installer_util',
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
        'chrome_launcher.cc',
        'chrome_launcher.h',
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
        'npapi_url_request.cc',
        'npapi_url_request.h',
        'ns_associate_iid_win.h',
        'ns_isupports_impl.h',
        'plugin_url_request.cc',
        'plugin_url_request.h',
        'scoped_ns_ptr_win.h',
        'script_security_manager.h',
        'sync_msg_reply_dispatcher.cc',
        'sync_msg_reply_dispatcher.h',
        'utils.cc',
        'utils.h',
      ],
    },
    {
      'target_name': 'chrome_launcher',
      'type': 'executable',
      'msvs_guid': 'B7E540C1-49D9-4350-ACBC-FB8306316D16',
      'dependencies': [],
      'sources': [
        'chrome_launcher_main.cc',
      ],
      'msvs_settings': {
        'VCLinkerTool': {
          'OutputFile':
              '$(OutDir)\\servers\\$(ProjectName).exe',
          # Set /SUBSYSTEM:WINDOWS since this is not a command-line program.
          'SubSystem': '2',
          # We're going for minimal size, so no standard library (in release
          # builds).
          'IgnoreAllDefaultLibraries': "true",
        },
        'VCCLCompilerTool': {
          # Requires standard library, so disable it.
          'BufferSecurityCheck': "false",
        },
      },
      'configurations': {
        # Bring back the standard library in debug buidls.
        'Debug_Base': {
          'msvs_settings': {
            'VCLinkerTool': {
              'IgnoreAllDefaultLibraries': "false",
            },
          },
        },
      },
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
        'resources/chrome_frame_strings.grd',
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
      'target_name': 'chrome_frame_ie',
      'type': 'static_library',
      'dependencies': [
        'chrome_frame_strings',
        '../chrome/chrome.gyp:common',
        '../chrome/chrome.gyp:utility',
        '../build/temp_gyp/googleurl.gyp:googleurl',

      ],
      'sources': [
        'bho.cc',
        'bho.h',
        'bho.rgs',
        'chrome_active_document.cc',
        'chrome_active_document.h',
        'chrome_active_document.rgs',
        'chrome_frame_activex.cc',
        'chrome_frame_activex.h',
        'chrome_frame_activex_base.h',
        'chrome_frame_activex.rgs',
        'chrome_frame_histograms.h',
        'chrome_frame_histograms.cc',
        'chrome_frame_reporting.cc',
        'chrome_frame_reporting.h',
        'chrome_protocol.cc',
        'chrome_protocol.h',
        'chrome_protocol.rgs',
        'chrome_tab.h',
        'chrome_tab.idl',
        'com_message_event.cc',
        'com_message_event.h',
        'com_type_info_holder.cc',
        'com_type_info_holder.h',
        'find_dialog.cc',
        'find_dialog.h',
        'function_stub.h',
        'http_negotiate.h',
        'http_negotiate.cc',
        'in_place_menu.h',
        'ole_document_impl.h',
        'protocol_sink_wrap.cc',
        'protocol_sink_wrap.h',
        'sync_msg_reply_dispatcher.cc',
        'sync_msg_reply_dispatcher.h',
        'extra_system_apis.h',
        'urlmon_url_request.cc',
        'urlmon_url_request.h',
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
            '../chrome/installer/installer.gyp:installer_util',
            '../google_update/google_update.gyp:google_update',
            # Crash Reporting
            'crash_reporting/crash_reporting.gyp:crash_report',
            'crash_reporting/crash_reporting.gyp:vectored_handler_tests',
          ],
        },],
      ],
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
        'chrome_launcher',
        'xulrunner_sdk',
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
        'chrome_tab_version.rc.version',
        'iids.cc',
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
            '<(SHARED_INTERMEDIATE_DIR)/chrome_frame/chrome_frame_strings.rc',
          ],
          'dependencies': [
            '../breakpad/breakpad.gyp:breakpad_handler',
            '../chrome/chrome.gyp:automation',
            # Make the archive build happy.
            '../chrome/chrome.gyp:syncapi',
            # Installer
            '../chrome/installer/installer.gyp:installer_util',
            '../google_update/google_update.gyp:google_update',
            # Crash Reporting
            'crash_reporting/crash_reporting.gyp:crash_report',
            'crash_reporting/crash_reporting.gyp:vectored_handler_tests',
          ],
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
      'rules': [
        # Borrowed from chrome.gyp:chrome_dll_version, branding references
        # removed
        {
          'rule_name': 'version',
          'extension': 'version',
          'variables': {
            'version_py': '../chrome/tools/build/version.py',
            'version_path': '../chrome/VERSION',
            'template_input_path': 'chrome_tab_version.rc.version',
          },
          'inputs': [
            '<(template_input_path)',
            '<(version_path)',
          ],
          'outputs': [
            '<(INTERMEDIATE_DIR)/chrome_tab_version.rc',
          ],
          'action': [
            'python',
            '<(version_py)',
            '-f', '<(version_path)',
            '<(template_input_path)',
            '<@(_outputs)',
          ],
          'process_outputs_as_sources': 1,
          'message': 'Generating version information in <(_outputs)'
        },
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
