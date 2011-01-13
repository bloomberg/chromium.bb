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
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/ceee/common/common.gyp:ceee_common',
        '<(DEPTH)/ceee/common/common.gyp:initializing_coclass',
        '<(DEPTH)/ceee/ie/common/common.gyp:ie_common_settings',
        '<(DEPTH)/chrome/chrome.gyp:installer_util',
        '<(DEPTH)/chrome_frame/chrome_frame.gyp:chrome_tab_idl',
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
        'ceee_ie_lib',
        'ie_toolband_common',
        'toolband_proxy_lib',
        'toolband_idl',
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/breakpad/breakpad.gyp:breakpad_handler',
        '<(DEPTH)/ceee/common/common.gyp:ceee_common',
        '<(DEPTH)/ceee/common/common.gyp:initializing_coclass',
        '<(DEPTH)/ceee/ie/broker/broker.gyp:broker_rpc_idl',
        '<(DEPTH)/ceee/ie/broker/broker.gyp:broker_rpc_lib',
        '<(DEPTH)/ceee/ie/common/common.gyp:ie_common_settings',
        '<(DEPTH)/ceee/ie/common/common.gyp:ie_guids',
        '<(DEPTH)/ceee/ie/plugin/bho/bho.gyp:bho',
        '<(DEPTH)/ceee/ie/plugin/scripting/scripting.gyp:scripting',
        '<(DEPTH)/ceee/ie/plugin/scripting/scripting.gyp:javascript_bindings',
        '<(DEPTH)/chrome/chrome.gyp:chrome_version_header',
        '<(DEPTH)/chrome_frame/chrome_frame.gyp:chrome_tab_idl',
      ],
      'conditions': [
        [ 'branding == "Chrome"', {
          'variables': {
             'brand_specific_resources':
                 '../../../internal/toolband/brand_specific_resources.rc',
          },
        }, { # else branding != "Chrome"
          'variables': {
             'brand_specific_resources': 'brand_specific_resources.rc',
          },
        }],
      ],
      'sources': [
        'resource.h',
        'tool_band.rgs',
        'toolband.def',
        'toolband.rc',
        'toolband_module.cc',
        '../bho/browser_helper_object.rgs',
        '../executor.rgs',
        '../scripting/content_script_manager.rc',
        '<(brand_specific_resources)',
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
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/breakpad/breakpad.gyp:breakpad_handler',
        '<(DEPTH)/chrome/chrome.gyp:chrome_version_header',
        '<(DEPTH)/chrome_frame/crash_reporting/'
            'crash_reporting.gyp:crash_report',
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
        'toolband.idl',
        '<(DEPTH)/ceee/ie/broker/broker_lib.idl',
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
        'toolband_idl',
        '<(DEPTH)/base/base.gyp:base',
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
