# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'includes': [
    '../../../../build/common.gypi',
  ],
  'targets': [
    {
      'target_name': 'ceee_ie_lib',
      'type': 'static_library',
      'dependencies': [
        '<(DEPTH)/chrome_frame/chrome_frame.gyp:chrome_tab_idl',
        '../../common/common.gyp:ie_common_settings',
        '../../../../base/base.gyp:base',
        '../../../../ceee/common/common.gyp:ceee_common',
        '../../../../ceee/common/common.gyp:initializing_coclass',
      ],
      'sources': [
        'tool_band.cc',
        'tool_band.h',
      ],
      'libraries': [
        'oleacc.lib',
        'iepmapi.lib',
      ],
      'include_dirs': [
        # For chrome_tab.h
        '<(SHARED_INTERMEDIATE_DIR)',
      ],
    },
    {
      'target_name': 'ceee_ie',
      'type': 'shared_library',
      'dependencies': [
        '../../broker/broker.gyp:broker_rpc_idl',
        '../../broker/broker.gyp:broker_rpc_lib',
        'ceee_ie_lib',
        'ie_toolband_common',
        'toolband_proxy_lib',
        'toolband_idl',
        '../bho/bho.gyp:bho',
        '../scripting/scripting.gyp:scripting',
        '../scripting/scripting.gyp:javascript_bindings',
        '../../common/common.gyp:ie_common_settings',
        '../../common/common.gyp:ie_guids',
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/breakpad/breakpad.gyp:breakpad_handler',
        '<(DEPTH)/ceee/common/common.gyp:ceee_common',
        '<(DEPTH)/ceee/common/common.gyp:initializing_coclass',
        '<(DEPTH)/chrome/chrome.gyp:chrome_version_header',
        '<(DEPTH)/chrome_frame/chrome_frame.gyp:chrome_tab_idl',
      ],
      'sources': [
        'resource.h',
        'tool_band.rgs',
        'toolband.def',
        'toolband.rc',
        'toolband_module.cc',
        '../bho/browser_helper_object.rgs',
        '../executor.rgs',
        '../executor_creator.rgs',
        '../scripting/content_script_manager.rc',
      ],
      'libraries': [
        'iepmapi.lib',
        'oleacc.lib',
        'rpcrt4.lib',
      ],
      'include_dirs': [
        # Allows us to include .tlb and .h files generated
        # from our .idl without undue trouble
        '$(IntDir)',
        # For version.h
        '<(SHARED_INTERMEDIATE_DIR)',
      ],
      'msvs_settings': {
        'VCLinkerTool': {
          'OutputFile': '$(OutDir)/servers/$(ProjectName).dll',
        },
      },
    },
    {
      'target_name': 'ie_toolband_common',
      'type': 'static_library',
      'dependencies': [
        '../../../../chrome_frame/crash_reporting/'
            'crash_reporting.gyp:crash_report',
        '../../../../base/base.gyp:base',
        '../../../../breakpad/breakpad.gyp:breakpad_handler',
        '<(DEPTH)/chrome/chrome.gyp:chrome_version_header',
      ],
      'sources': [
        'toolband_module_reporting.cc',
        'toolband_module_reporting.h',
      ],
      'include_dirs': [
        # For version.h
        '<(SHARED_INTERMEDIATE_DIR)',
      ],
    },
    {
      'target_name': 'toolband_idl',
      'type': 'none',
      'sources': [
        '../../broker/broker_lib.idl',
        'toolband.idl',
      ],
      'msvs_settings': {
        'VCMIDLTool': {
          'OutputDirectory': '<(SHARED_INTERMEDIATE_DIR)',
          'DLLDataFileName': '$(InputName)_dlldata.c',
        },
      },
      # Add the output dir for those who depend on us.
      'direct_dependent_settings': {
        'include_dirs': ['<(SHARED_INTERMEDIATE_DIR)'],
      },
    },
    {
      # This target builds a library out of the toolband proxy/stubs.
      # TODO(siggi): Rename the IDL and move it to ie/plugin/common, as
      #     it's defining interfaces that are common across the the broker
      #     and bho/executor.
      'target_name': 'toolband_proxy_lib',
      'type': 'static_library',
      'sources': [
        'toolband_proxy.h',
        'toolband_proxy.cc',
        'toolband_proxy.rgs',
        '<(SHARED_INTERMEDIATE_DIR)/toolband_p.c',
        '<(SHARED_INTERMEDIATE_DIR)/toolband_dlldata.c',
      ],
      'dependencies': [
        '../../../../base/base.gyp:base',
        'toolband_idl',
      ],
      'defines': [
        # This define adds ToolbandProxy as a prefix to the generated
        # proxy/stub entrypoint routine names.
        'ENTRY_PREFIX=ToolbandProxy',
        # The proxy stub code defines _purecall, which conflicts with our
        # CRT, so we neuter this here.
        '_purecall=ToolbandPureCall',
      ],
    },
  ]
}
