# Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
          '<(DEPTH)\\third_party\\python_26\\setup_env.bat && python'
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
      # locales need to be built for the chrome frame binaries to be loadable.
      '../chrome/app/locales/locales.gyp:*',
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
      # Builds the crash tests in crash_reporting.
      'target_name': 'chrome_frame_crash_tests',
      'type': 'none',
      'dependencies': [
        'crash_reporting/crash_reporting.gyp:minidump_test',
        'crash_reporting/crash_reporting.gyp:vectored_handler_tests',
      ],
    },
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
      # Builds our IDL file to the shared intermediate directory.
      'target_name': 'chrome_tab_idl',
      'type': 'none',
      'msvs_settings': {
        'VCMIDLTool': {
          'OutputDirectory': '<(SHARED_INTERMEDIATE_DIR)',
        },
      },
      'sources': [
        'chrome_tab.idl',
      ],
      # Add the output dir for those who depend on us.
      'direct_dependent_settings': {
        'include_dirs': ['<(SHARED_INTERMEDIATE_DIR)'],
      },
    },
    {
      'target_name': 'chrome_frame_privileged_mock',
      'type': 'none',
      'dependencies': [
        'chrome_tab_idl',
      ],
      'sources': [
        '../ceee/testing/utils/com_mock.py',
        '<(SHARED_INTERMEDIATE_DIR)/chrome_tab.h',
      ],
      'actions': [
        {
          'action_name': 'make_chrome_frame_privileged_mock',
          'msvs_cygwin_shell': 0,
          'msvs_quote_cmd': 0,
          'inputs': [
            '../ceee/testing/utils/com_mock.py',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/mock_ichromeframeprivileged.gen',
          ],
          'action': [
            '<@(python)',
            '../ceee/testing/utils/com_mock.py',
            'IChromeFramePrivileged',
            '<(SHARED_INTERMEDIATE_DIR)/chrome_tab.h',
            '> "<(SHARED_INTERMEDIATE_DIR)/mock_ichromeframeprivileged.gen"',
          ],
        },
      ],
      # All who use this need to be able to find the .gen file we generate.
      'all_dependent_settings': {
        'include_dirs': ['<(SHARED_INTERMEDIATE_DIR)'],
      },
    },
    {
      'target_name': 'chrome_frame_unittests',
      'type': 'executable',
      'dependencies': [
        '../base/base.gyp:test_support_base',
        '../ceee/ie/common/common.gyp:ie_common',
        '../ceee/testing/utils/test_utils.gyp:test_utils',
        '../chrome/chrome.gyp:policy',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        'chrome_frame_ie',
        'chrome_frame_privileged_mock',
        'chrome_frame_strings',
        'chrome_tab_idl',
        'xulrunner_sdk',
      ],
      'sources': [
        '<(SHARED_INTERMEDIATE_DIR)/chrome_tab.h',
        'chrome_frame_histograms.h',
        'chrome_frame_histograms.cc',
        'chrome_frame_unittest_main.cc',
        'chrome_launcher.cc',
        'chrome_launcher.h',
        'chrome_launcher_unittest.cc',
        'function_stub_unittest.cc',
        'renderer_glue.cc',
        'test/chrome_frame_activex_unittest.cc',
        'test/chrome_tab_mocks.h',
        'test/chrome_frame_test_utils.h',
        'test/chrome_frame_test_utils.cc',
        'test/com_message_event_unittest.cc',
        'test/dll_redirector_test.cc',
        'test/exception_barrier_unittest.cc',
        'test/html_util_unittests.cc',
        'test/http_negotiate_unittest.cc',
        'test/infobar_unittests.cc',
        'test/policy_settings_unittest.cc',
        'test/ready_mode_unittest.cc',
        'test/simulate_input.h',
        'test/simulate_input.cc',
        'test/urlmon_moniker_tests.h',
        'test/urlmon_moniker_unittest.cc',
        'test/util_unittests.cc',
        'test/win_event_receiver.h',
        'test/win_event_receiver.cc',
        'unittest_precompile.h',
        'unittest_precompile.cc',
        'urlmon_upload_data_stream.cc',
        'urlmon_upload_data_stream_unittest.cc',
        'vtable_patch_manager_unittest.cc',
      ],
      'include_dirs': [
        '<@(xul_include_directories)',
      ],
      'resource_include_dirs': [
        '<(INTERMEDIATE_DIR)',
        '<(SHARED_INTERMEDIATE_DIR)',
      ],
      'conditions': [
        # We needed to extract this test from the chrome_frame_unittests because
        # we can't instrument code for coverage if it depends on 3rd party
        # binaries that we don't have PDBs for. See here for more details:
        # http://connect.microsoft.com/VisualStudio/feedback/details/176188/can-not-disable-warning-lnk4099
        ['coverage==0', {
          'dependencies': [
            'chrome_frame_npapi',
          ],
          'sources': [
            'chrome_frame_npapi_unittest.cc',
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
              ],
            }],
          ],
        }],
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
        ['OS=="win"', {
          'link_settings': {
            'libraries': [
              '-lshdocvw.lib', '-loleacc.lib',
            ],
          },
          'msvs_settings': {
            'VCLinkerTool': {
              'DelayLoadDLLs': ['shdocvw.dll'],
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
        '../base/base.gyp:test_support_base',
        '../build/temp_gyp/googleurl.gyp:googleurl',
        '../chrome/chrome.gyp:chrome_gpu',
        '../chrome/chrome.gyp:chrome_version_header',
        '../chrome/chrome.gyp:common',
        '../chrome/chrome.gyp:utility',
        '../chrome/chrome.gyp:browser',
        '../chrome/chrome.gyp:debugger',
        '../chrome/chrome.gyp:renderer',
        '../net/net.gyp:net_test_support',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        '../third_party/iaccessible2/iaccessible2.gyp:iaccessible2',
        '../third_party/iaccessible2/iaccessible2.gyp:IAccessible2Proxy',
        '../third_party/libxslt/libxslt.gyp:libxslt',
        'chrome_frame_ie',
        'chrome_frame_npapi',
        'chrome_frame_strings',
        'chrome_frame_utils',
        'chrome_tab_idl',
        'npchrome_frame',
      ],
      'sources': [
        '../base/test/test_suite.h',
        'cfproxy_test.cc',
        'external_tab_test.cc',
        'test/automation_client_mock.cc',
        'test/automation_client_mock.h',
        'test/chrome_frame_test_utils.cc',
        'test/chrome_frame_test_utils.h',
        'test/chrome_frame_ui_test_utils.cc',
        'test/chrome_frame_ui_test_utils.h',
        'test/chrome_frame_automation_mock.cc',
        'test/chrome_frame_automation_mock.h',
        'test/delete_chrome_history_test.cc',
        'test/header_test.cc',
        'test/ie_event_sink.cc',
        'test/ie_event_sink.h',
        'test/mock_ie_event_sink_actions.h',
        'test/mock_ie_event_sink_test.cc',
        'test/mock_ie_event_sink_test.h',
        'test/navigation_test.cc',
        'test/proxy_factory_mock.cc',
        'test/proxy_factory_mock.h',
        'test/run_all_unittests.cc',
        'test/simple_resource_loader_test.cc',
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
        'test/win_event_receiver.cc',
        'test/win_event_receiver.h',
        'chrome_launcher_version.rc',
        '<(SHARED_INTERMEDIATE_DIR)/chrome_tab.h',
        'test_utils.cc',
        'test_utils.h',
      ],
      'include_dirs': [
        '<@(xul_include_directories)',
        '<(DEPTH)/third_party/wtl/include',
      ],
      'resource_include_dirs': [
        '<(INTERMEDIATE_DIR)',
      ],
      'conditions': [
        ['OS=="win"', {
          'link_settings': {
            'libraries': [
              '-loleacc.lib',
            ],
          },
          'msvs_settings': {
            'VCLinkerTool': {
              'DelayLoadDLLs': ['xpcom.dll', 'nspr4.dll'],
            },
          },
          'dependencies': [
            '../chrome/chrome.gyp:crash_service',
            '../chrome/chrome.gyp:automation',
            '../chrome/chrome.gyp:installer_util',
            '../google_update/google_update.gyp:google_update',
          ],
          'configurations': {
            'Debug_Base': {
              'msvs_settings': {
                'VCLinkerTool': {
                  'LinkIncremental': '<(msvs_debug_link_nonincremental)',
                },
              },
            },
          },
          'conditions': [
            ['win_use_allocator_shim==1', {
              'dependencies': [
                '../base/allocator/allocator.gyp:allocator',
              ],
            }],
          ],
        }],
      ],
    },
    {
      'target_name': 'chrome_frame_perftests',
      'msvs_guid': '3767888B-76ED-4D2A-B1F5-263CC56A12AA',
      'type': 'executable',
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:base_i18n',
        '../base/base.gyp:test_support_base',
        '../build/temp_gyp/googleurl.gyp:googleurl',
        '../chrome/chrome.gyp:chrome_gpu',
        '../chrome/chrome.gyp:common',
        '../chrome/chrome.gyp:browser',
        '../chrome/chrome.gyp:debugger',
        '../chrome/chrome.gyp:test_support_ui',
        '../chrome/chrome.gyp:utility',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        '../third_party/libxml/libxml.gyp:libxml',
        '../third_party/libxslt/libxslt.gyp:libxslt',
        'chrome_frame_ie',
        'chrome_frame_npapi',
        'chrome_frame_strings',
        'chrome_frame_utils',
        'chrome_tab_idl',
        'npchrome_frame',
      ],
      'sources': [
        '../base/perf_test_suite.h',
        '../base/perftimer.cc',
        '../base/test/test_file_util.h',
        '../chrome/test/chrome_process_util.cc',
        '../chrome/test/chrome_process_util.h',
        '../chrome/test/ui/ui_test.cc',
        '<(SHARED_INTERMEDIATE_DIR)/chrome_tab.h',
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
        'test/win_event_receiver.cc',
        'test/win_event_receiver.h',
      ],
      'include_dirs': [
        '<@(xul_include_directories)',
        '<(DEPTH)/third_party/wtl/include',
      ],
      'conditions': [
        ['OS=="win"', {
          'configurations': {
            'Debug_Base': {
              'msvs_settings': {
                'VCLinkerTool': {
                  'LinkIncremental': '<(msvs_debug_link_nonincremental)',
                },
              },
            },
          },
          'link_settings': {
            'libraries': [
              '-loleacc.lib',
            ],
          },
          'msvs_settings': {
            'VCLinkerTool': {
              'IgnoreDefaultLibraryNames': ['nspr.lib', 'nspr4.lib'],
            },
          },
          'dependencies': [
            '../breakpad/breakpad.gyp:breakpad_handler',
            '../chrome/chrome.gyp:automation',
            '../chrome/chrome.gyp:crash_service',
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
        '../chrome/chrome.gyp:chrome_gpu',
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
        'chrome_tab_idl',
        'npchrome_frame',
      ],
      'sources': [
        '../net/url_request/url_request_unittest.cc',
        'test/chrome_frame_test_utils.cc',
        'test/chrome_frame_test_utils.h',
        'test/simulate_input.cc',
        'test/simulate_input.h',
        'test/test_server.cc',
        'test/test_server.h',
        'test/win_event_receiver.cc',
        'test/win_event_receiver.h',
        'test/net/fake_external_tab.cc',
        'test/net/fake_external_tab.h',
        'test/net/process_singleton_subclass.cc',
        'test/net/process_singleton_subclass.h',
        'test/net/test_automation_provider.cc',
        'test/net/test_automation_provider.h',
        'test/net/test_automation_resource_message_filter.cc',
        'test/net/test_automation_resource_message_filter.h',
        '<(SHARED_INTERMEDIATE_DIR)/chrome_tab.h',
      ],
      'conditions': [
        ['OS=="win"', {
          'link_settings': {
            'libraries': [
              '-loleacc.lib',
            ],
          },
          'msvs_settings': {
            'VCLinkerTool': {
              'DelayLoadDLLs': ['prntvpt.dll'],
            },
          },
          'dependencies': [
            '../breakpad/breakpad.gyp:breakpad_handler',
            '../chrome/chrome.gyp:automation',
            '../chrome/chrome.gyp:crash_service',
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
        '../base/base.gyp:base',
        '../base/base.gyp:test_support_base',
        '../chrome/chrome.gyp:browser',
        '../chrome/chrome.gyp:chrome_gpu',
        '../chrome/chrome.gyp:debugger',
        '../chrome/chrome.gyp:renderer',
        '../chrome/chrome.gyp:test_support_common',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        'chrome_frame_ie',
        'chrome_frame_npapi',
        'chrome_frame_strings',
        'chrome_tab_idl',
      ],
      'sources': [
        'test/reliability/run_all_unittests.cc',
        'test/reliability/page_load_test.cc',
        'test/reliability/page_load_test.h',
        'test/reliability/reliability_test_suite.h',
        'test/chrome_frame_test_utils.cc',
        'test/chrome_frame_test_utils.h',
        'test/ie_event_sink.cc',
        'test/ie_event_sink.h',
        'test_utils.cc',
        'test_utils.h',
        'test/simulate_input.cc',
        'test/simulate_input.h',
        'test/win_event_receiver.cc',
        'test/win_event_receiver.h',
        '<(SHARED_INTERMEDIATE_DIR)/chrome_tab.h',
        '../base/test/test_file_util_win.cc',
        '../chrome/test/automation/proxy_launcher.cc',
        '../chrome/test/automation/proxy_launcher.h',
        '../chrome/test/ui/ui_test.cc',
        '../chrome/test/ui/ui_test.h',
        '../chrome/test/ui/ui_test_suite.cc',
        '../chrome/test/ui/ui_test_suite.h',
        '../chrome/test/chrome_process_util.cc',
        '../chrome/test/chrome_process_util.h',
      ],
      'resource_include_dirs': [
        '<(INTERMEDIATE_DIR)',
      ],
      'conditions': [
        ['OS=="win"', {
          'link_settings': {
            'libraries': [
              '-loleacc.lib',
            ],
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
      'target_name': 'chrome_frame_qa_tests',
      'msvs_guid': 'D6B3174D-31DD-49D6-83C0-A63A6A135E0E',
      'type': 'executable',
      'dependencies': [
        '../base/base.gyp:test_support_base',
        '../build/temp_gyp/googleurl.gyp:googleurl',
        '../net/net.gyp:net_test_support',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        '../third_party/iaccessible2/iaccessible2.gyp:iaccessible2',
        'chrome_frame_ie',
        'chrome_frame_npapi',
        'chrome_frame_strings',
        'npchrome_frame',
      ],
      'sources': [
        '../base/test/test_suite.h',
        'test/chrome_frame_test_utils.cc',
        'test/chrome_frame_test_utils.h',
        'test/chrome_frame_ui_test_utils.cc',
        'test/chrome_frame_ui_test_utils.h',
        'test/external_sites_test.cc',
        'test/ie_event_sink.cc',
        'test/ie_event_sink.h',
        'test/mock_ie_event_sink_actions.h',
        'test/mock_ie_event_sink_test.cc',
        'test/mock_ie_event_sink_test.h',
        'test/run_all_unittests.cc',
        'test/simulate_input.cc',
        'test/simulate_input.h',
        'test/test_server.cc',
        'test/test_server.h',
        'test/test_with_web_server.cc',
        'test/test_with_web_server.h',
        'test/win_event_receiver.cc',
        'test/win_event_receiver.h',
        '<(SHARED_INTERMEDIATE_DIR)/chrome_tab.h',
        'renderer_glue.cc',
        'test_utils.cc',
        'test_utils.h',
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
          'link_settings': {
            'libraries': [
              '-loleacc.lib',
            ],
          },
          'msvs_settings': {
            'VCLinkerTool': {
              'DelayLoadDLLs': ['xpcom.dll', 'nspr4.dll'],
            },
          },
          'dependencies': [
            '../chrome/chrome.gyp:crash_service',
            '../chrome/chrome.gyp:automation',
            '../chrome/chrome.gyp:installer_util',
            '../google_update/google_update.gyp:google_update',
          ]
        }],
      ],
    },
    {
      'target_name': 'chrome_frame_npapi_core',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
      ],
      'sources': [
        'np_browser_functions.cc',
        'np_browser_functions.h',
      ],
    },
    {
      'target_name': 'chrome_frame_npapi',
      'type': 'static_library',
      'dependencies': [
        'chrome_frame_common',
        'chrome_frame_npapi_core',
        'chrome_frame_strings',
        'chrome_frame_utils',
        '../chrome/chrome.gyp:common',
      ],
      'sources': [
        'chrome_frame_npapi.cc',
        'chrome_frame_npapi.h',
        'ff_30_privilege_check.cc',
        'ff_privilege_check.h',
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
        'scoped_ns_ptr_win.h',
      ],
      'include_dirs': [
        '<@(xul_include_directories)',
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
        '../breakpad/breakpad.gyp:breakpad_handler',
        '../chrome/chrome.gyp:chrome_version_header',
      ],
      'include_dirs': [
        # To allow including "version.h"
        '<(SHARED_INTERMEDIATE_DIR)',
      ],
      'sources': [
        'crash_server_init.cc',
        'crash_server_init.h',
        'simple_resource_loader.cc',
        'simple_resource_loader.h',
      ],
    },
    {
      'target_name': 'chrome_frame_ie',
      'type': 'static_library',
      'dependencies': [
        'chrome_frame_common',
        'chrome_frame_strings',
        'chrome_frame_utils',
        'chrome_tab_idl',
        '../ceee/ie/common/common.gyp:ie_common',
        '../ceee/ie/common/common.gyp:ie_guids',
        '../chrome/chrome.gyp:common',
        '../chrome/chrome.gyp:policy',
        '../chrome/chrome.gyp:utility',
        '../build/temp_gyp/googleurl.gyp:googleurl',
        '../third_party/libxml/libxml.gyp:libxml',
        '../third_party/bzip2/bzip2.gyp:bzip2',
        '../webkit/support/webkit_support.gyp:webkit_user_agent',
      ],
      'sources': [
        'bho.cc',
        'bho.h',
        'bho.rgs',
        'bind_context_info.cc',
        'bind_context_info.h',
        'bind_status_callback_impl.cc',
        'bind_status_callback_impl.h',
        'buggy_bho_handling.cc',
        'buggy_bho_handling.h',
        'chrome_active_document.cc',
        'chrome_active_document.h',
        'chrome_active_document.rgs',
        'chrome_frame_activex.cc',
        'chrome_frame_activex.h',
        'chrome_frame_activex.rgs',
        'chrome_frame_activex_base.h',
        'chrome_frame_histograms.cc',
        'chrome_frame_histograms.h',
        'chrome_protocol.cc',
        'chrome_protocol.h',
        'chrome_protocol.rgs',
        '<(SHARED_INTERMEDIATE_DIR)/chrome_tab.h',
        'com_message_event.cc',
        'com_message_event.h',
        'com_type_info_holder.cc',
        'com_type_info_holder.h',
        'delete_chrome_history.cc',
        'delete_chrome_history.h',
        'dll_redirector.cc',
        'dll_redirector.h',
        'exception_barrier.cc',
        'exception_barrier.h',
        'exception_barrier_lowlevel.asm',
        'find_dialog.cc',
        'find_dialog.h',
        'function_stub.cc',
        'function_stub.h',
        'html_utils.h',
        'html_utils.cc',
        'http_negotiate.cc',
        'http_negotiate.h',
        'iids.cc',
        'in_place_menu.h',
        'infobars/infobar_content.h',
        'infobars/internal/displaced_window_manager.cc',
        'infobars/internal/displaced_window_manager.h',
        'infobars/internal/host_window_manager.cc',
        'infobars/internal/host_window_manager.h',
        'infobars/internal/infobar_window.cc',
        'infobars/internal/infobar_window.h',
        'infobars/internal/subclassing_window_with_delegate.h',
        'infobars/infobar_manager.h',
        'infobars/infobar_manager.cc',
        'metrics_service.cc',
        'metrics_service.h',
        'ole_document_impl.h',
        'policy_settings.cc',
        'policy_settings.h',
        'protocol_sink_wrap.cc',
        'protocol_sink_wrap.h',
        'ready_mode/internal/ready_mode_state.h',
        'ready_mode/internal/ready_mode_web_browser_adapter.cc',
        'ready_mode/internal/ready_mode_web_browser_adapter.h',
        'ready_mode/internal/ready_prompt_content.cc',
        'ready_mode/internal/ready_prompt_content.h',
        'ready_mode/internal/ready_prompt_window.cc',
        'ready_mode/internal/ready_prompt_window.h',
        'ready_mode/internal/registry_ready_mode_state.cc',
        'ready_mode/internal/registry_ready_mode_state.h',
        'ready_mode/internal/url_launcher.h',
        'ready_mode/ready_mode.cc',
        'ready_mode/ready_mode.h',
        'register_bho.rgs',
        'stream_impl.cc',
        'stream_impl.h',
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
        'utils.h',
        'utils.cc',
        'vtable_patch_manager.cc',
        'vtable_patch_manager.h',
      ],
      'include_dirs': [
        '<(INTERMEDIATE_DIR)/../chrome_frame',
        '<(DEPTH)/third_party/wtl/include',
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
          'message':
              'Assembling <(RULE_INPUT_PATH) to ' \
              '<(INTERMEDIATE_DIR)\<(RULE_INPUT_ROOT).obj.',
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
      'target_name': 'chrome_frame_common',
      'type': 'static_library',
      'sources': [
        'cfproxy.h',
        'cfproxy_private.h',
        'cfproxy_factory.cc',
        'cfproxy_proxy.cc',
        'cfproxy_support.cc',
        'chrome_frame_automation.h',
        'chrome_frame_automation.cc',
        'chrome_frame_delegate.h',
        'chrome_frame_delegate.cc',
        'chrome_frame_plugin.h',
        'chrome_launcher_utils.cc',
        'chrome_launcher_utils.h',
        'custom_sync_call_context.h',
        'external_tab.h',
        'external_tab.cc',
        'navigation_constraints.h',
        'navigation_constraints.cc',
        'plugin_url_request.h',
        'plugin_url_request.cc',
        'sync_msg_reply_dispatcher.h',
        'sync_msg_reply_dispatcher.cc',
        'task_marshaller.h',
        'task_marshaller.cc',
      ]
    },
    {
      'target_name': 'npchrome_frame',
      'type': 'shared_library',
      'msvs_guid': 'E3DE7E63-D3B6-4A9F-BCC4-5C8169E9C9F2',
      'dependencies': [
        '../base/base.gyp:base',
        'chrome_frame_ie',
        'chrome_frame_npapi',
        'chrome_frame_strings',
        'chrome_frame_utils',
        'chrome_tab_idl',
        'xulrunner_sdk',
        'chrome_frame_launcher.gyp:chrome_launcher',
        '../build/temp_gyp/googleurl.gyp:googleurl',
        'chrome_frame_launcher.gyp:chrome_frame_helper',
        'chrome_frame_launcher.gyp:chrome_frame_helper_dll',
        '../chrome/chrome.gyp:chrome',
        '../chrome/chrome.gyp:chrome_dll',
        '../chrome/chrome.gyp:chrome_dll_version',
        '../chrome/chrome.gyp:common',
        '../chrome/chrome.gyp:utility',
      ],
      'sources': [
        'chrome_frame_npapi.rgs',
        'chrome_frame_npapi_entrypoints.cc',
        'chrome_frame_npapi_entrypoints.h',
        'chrome_frame_reporting.cc',
        'chrome_frame_reporting.h',
        'chrome_tab.cc',
        'chrome_tab.def',
        '<(SHARED_INTERMEDIATE_DIR)/chrome_tab.h',
        '<(SHARED_INTERMEDIATE_DIR)/chromet_tab.tlb',
        'chrome_tab.rgs',
        'chrome_tab_version.rc',
        'renderer_glue.cc',
        'resource.h',
      ],
      'include_dirs': [
        # For chrome_tab.h
        '<(SHARED_INTERMEDIATE_DIR)',
        '<(INTERMEDIATE_DIR)/../npchrome_frame',
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
            '../breakpad/breakpad.gyp:breakpad_handler_dll',
            '../chrome/chrome.gyp:automation',
            # Make the archive build happy.
            '../chrome/chrome.gyp:syncapi',
            # Installer
            '../chrome/chrome.gyp:installer_util',
            '../google_update/google_update.gyp:google_update',
            # Crash Reporting
            'crash_reporting/crash_reporting.gyp:crash_report',
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
  'conditions': [
    # To enable the coverage targets, do
    #    GYP_DEFINES='coverage=1' gclient sync
    ['coverage!=0',
      { 'targets': [
        {
          # Coverage BUILD AND RUN.
          # Not named coverage_build_and_run for historical reasons.
          'target_name': 'gcf_coverage',
          'dependencies': [ 'gcf_coverage_build', 'gcf_coverage_run' ],
          # do NOT place this in the 'all' list; most won't want it.
          'suppress_wildcard': 1,
          'type': 'none',
          'actions': [
            {
              'message': 'Coverage is now complete.',
              # MSVS must have an input file and an output file.
              'inputs': [ '<(PRODUCT_DIR)/gcf_coverage.info' ],
              'outputs': [ '<(PRODUCT_DIR)/gcf_coverage-build-and-run.stamp' ],
              'action_name': 'gcf_coverage',
              # Wish gyp had some basic builtin commands (e.g. 'touch').
              'action': [ 'python', '-c',
                          'import os; ' \
                          'open(' \
                          '\'<(PRODUCT_DIR)\' + os.path.sep + ' \
                          '\'gcf_coverage-build-and-run.stamp\'' \
                          ', \'w\').close()' ],
              # Use outputs of this action as inputs for the main target build.
              # Seems as a misnomer but makes this happy on Linux (scons).
              'process_outputs_as_sources': 1,
            },
          ],
        },
        # Coverage BUILD.  Compile only; does not run the bundles.
        # Intended as the build phase for our coverage bots.
        #
        # Builds unit test bundles needed for coverage.
        # Outputs this list of bundles into coverage_bundles.py.
        #
        # If you want to both build and run coverage from your IDE,
        # use the 'coverage' target.
        {
          'target_name': 'gcf_coverage_build',
          'suppress_wildcard': 1,
          'type': 'none',
          'dependencies': [
            # Some tests are disabled because they depend on browser.lib which
            # has some trouble to link with instrumentation. Until this is
            # fixed on the Chrome side we won't get complete coverage from
            # our tests but we at least get the process rolling...
            # TODO(mad): FIX THIS!
            #'chrome_frame_net_tests',
            #'chrome_frame_reliability_tests',

            # Other tests depend on Chrome bins being available when they run.
            # Those should be re-enabled as soon as we setup the build slave to
            # also build (or download an archive of) Chrome, even it it isn't
            # instrumented itself.
            # TODO(mad): FIX THIS!
            #'chrome_frame_perftests',
            #'chrome_frame_tests',

            'chrome_frame_unittests',
          ],  # 'dependencies'
          'actions': [
            {
              # TODO(jrg):
              # Technically I want inputs to be the list of
              # executables created in <@(_dependencies) but use of
              # that variable lists the dep by dep name, not their
              # output executable name.
              # Is there a better way to force this action to run, always?
              #
              # If a test bundle is added to this coverage_build target it
              # necessarily means this file (chrome_frame.gyp) is changed,
              # so the action is run (coverage_bundles.py is generated).
              # Exceptions to that rule are theoretically possible
              # (e.g. re-gyp with a GYP_DEFINES set).
              # Else it's the same list of bundles as last time.  They are
              # built (since on the deps list) but the action may not run.
              # For now, things work, but it's less than ideal.
              'inputs': [ 'chrome_frame.gyp' ],
              'outputs': [ '<(PRODUCT_DIR)/coverage_bundles.py' ],
              'action_name': 'gcf_coverage_build',
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
          ],
        },
        # Coverage RUN.  Does not actually compile the bundles (though it
        # depends on the gcf_coverage_build step which will do it).
        # This target mirrors the run_coverage_bundles buildbot phase.
        # If you update this command update the mirror in
        # $BUILDBOT/scripts/master/factory/chromium_commands.py.
        # If you want both build and run, use the 'gcf_coverage' target which
        # adds a bit more magic to identify if we need to run or not.
        {
          'target_name': 'gcf_coverage_run',
          'dependencies': [ 'gcf_coverage_build' ],
          'suppress_wildcard': 1,
          'type': 'none',
          'actions': [
            {
              # MSVS must have an input file and an output file.
              'inputs': [ '<(PRODUCT_DIR)/coverage_bundles.py' ],
              'outputs': [ '<(PRODUCT_DIR)/coverage.info' ],
              'action_name': 'gcf_coverage_run',
              'action': [ 'python',
                          '../tools/code_coverage/coverage_posix.py',
                          '--directory',
                          '<(PRODUCT_DIR)',
                          '--src_root',
                          '.',
                          '--bundles',
                          '<(PRODUCT_DIR)/coverage_bundles.py',
                        ],
              # Use outputs of this action as inputs for the main target build.
              # Seems as a misnomer but makes this happy on Linux (scons).
              'process_outputs_as_sources': 1,
            },
          ],
        },
      ],
    }, ],  # 'coverage!=0'
  ],  # 'conditions'
}

# vim: shiftwidth=2:et:ai:tabstop=2

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
