# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'includes': [
    '../../../build/common.gypi',
  ],
  'targets': [
    {
      'target_name': 'broker_rpc_idl',
      'type': 'none',
      'sources': [
        'broker_rpc_lib.idl',
      ],
      'msvs_settings': {
        'VCMIDLTool': {
          'OutputDirectory': '<(SHARED_INTERMEDIATE_DIR)',
          'DLLDataFileName': '$(InputName)_dlldata.c',
          # Prevent middle to insert 'unsigned' before 'char' in generated
          # *.h and *.c files.
          'DefaultCharType': '0',
          'AdditionalOptions': '/prefix all "BrokerRpcClient_" '
                               'server "BrokerRpcServer_"'
        },
      },
      # Add the output dir for those who depend on us.
      'direct_dependent_settings': {
        'include_dirs': ['<(SHARED_INTERMEDIATE_DIR)'],
      },
    },
    {
      'target_name': 'broker_rpc_lib',
      'type': 'static_library',
      'dependencies': [
        'broker_rpc_idl',
      ],
      'sources': [
        '<(SHARED_INTERMEDIATE_DIR)/broker_rpc_lib_c.c',
        '<(SHARED_INTERMEDIATE_DIR)/broker_rpc_lib_s.c',
      ],
      'msvs_settings': {
        'VCCLCompilerTool': {
          'UsePrecompiledHeader': '0',
          'ForcedIncludeFiles': '$(NOINHERIT)',
        },
      },
    },
    {
      'target_name': 'broker',
      'type': 'static_library',
      'dependencies': [
        'broker_rpc_idl',
        '../common/common.gyp:ie_common',
        '../common/common.gyp:ie_common_settings',
        '../plugin/toolband/toolband.gyp:toolband_idl',
        '../../../base/base.gyp:base',
        '../../../build/temp_gyp/googleurl.gyp:googleurl',
        '../../../ceee/common/common.gyp:initializing_coclass',
        '../../../ceee/common/common.gyp:ceee_common',
        '<(DEPTH)/chrome_frame/chrome_frame.gyp:chrome_tab_idl',
        # Metrics service requires this.
        '<(DEPTH)/webkit/support/webkit_support.gyp:webkit_user_agent',
      ],
      'sources': [
        'api_dispatcher.cc',
        'api_dispatcher.h',
        'api_dispatcher_docs.h',
        'api_module_constants.cc',
        'api_module_constants.h',
        'api_module_util.cc',
        'api_module_util.h',
        'broker.cc',
        'broker.h',
        'broker_docs.h',
        'broker_rpc_client.cc',
        'broker_rpc_client.h',
        'broker_rpc_server.cc',
        'broker_rpc_server.h',
        'broker_rpc_utils.cc',
        'broker_rpc_utils.h',
        'chrome_postman.cc',
        'chrome_postman.h',
        'common_api_module.cc',
        'common_api_module.h',
        'cookie_api_module.cc',
        'cookie_api_module.h',
        'event_dispatching_docs.h',
        'executors_manager.cc',
        'executors_manager.h',
        'executors_manager_docs.h',
        'infobar_api_module.cc',
        'infobar_api_module.h',
        'tab_api_module.cc',
        'tab_api_module.h',
        'webnavigation_api_module.cc',
        'webnavigation_api_module.h',
        'webrequest_api_module.cc',
        'webrequest_api_module.h',
        'window_api_module.cc',
        'window_api_module.h',
        'window_events_funnel.cc',
        'window_events_funnel.h',
        # For chrome metrics service
        '<(DEPTH)/chrome_frame/metrics_service.cc',
        '<(DEPTH)/chrome_frame/metrics_service.h',
        '<(DEPTH)/chrome_frame/renderer_glue.cc',
      ],
      'include_dirs': [
        # For chrome_tab.h
        '<(SHARED_INTERMEDIATE_DIR)',
      ],
    },
    {
      # IF YOU CHANGE THIS TARGET NAME YOU MUST UPDATE:
      # ceee_module_util::kCeeeBrokerModuleName
      'target_name': 'ceee_broker',
      'type': 'executable',
      'sources': [
        'broker.rgs',
        'broker_module.cc',
        'broker_module.rc',
        'broker_module.rgs',
        'broker_module_util.h',
        'resource.h'
      ],
      'dependencies': [
        'broker',
        'broker_rpc_lib',
        '../common/common.gyp:ie_common_settings',
        '../common/common.gyp:ie_guids',
        '../plugin/toolband/toolband.gyp:toolband_idl',
        '../../../base/base.gyp:base',
        '../../../breakpad/breakpad.gyp:breakpad_handler',
        '../../../ceee/common/common.gyp:ceee_common',
        '<(DEPTH)/chrome/chrome.gyp:chrome_version_header',
        '<(DEPTH)/chrome_frame/chrome_frame.gyp:chrome_frame_ie',  # for GUIDs.
        '<(DEPTH)/chrome_frame/chrome_frame.gyp:chrome_tab_idl',
        '<(DEPTH)/chrome/chrome.gyp:common',
      ],
      'msvs_settings': {
        'VCLinkerTool': {
          'OutputFile': '$(OutDir)/servers/$(ProjectName).exe',
          # Set /SUBSYSTEM:WINDOWS since this is not a command-line program.
          'SubSystem': '2',
        },
      },
      'include_dirs': [
        # For version.h
        '<(SHARED_INTERMEDIATE_DIR)',
      ],
      'libraries': [
        'oleacc.lib',
        'iepmapi.lib',
        'rpcrt4.lib',
      ],
    },
  ]
}
