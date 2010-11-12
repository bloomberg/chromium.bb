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
      'target_name': 'broker',
      'type': 'static_library',
      'dependencies': [
        '../common/common.gyp:ie_common',
        '../common/common.gyp:ie_common_settings',
        '../plugin/toolband/toolband.gyp:toolband_idl',
        '../../../base/base.gyp:base',
        '../../../build/temp_gyp/googleurl.gyp:googleurl',
        '../../../ceee/common/common.gyp:initializing_coclass',
        '../../../ceee/common/common.gyp:ceee_common',
        '<(DEPTH)/chrome_frame/chrome_frame.gyp:chrome_tab_idl',
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
        '../common/precompile.cc',
        '../common/precompile.h',
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
      ],
      'configurations': {
        'Debug': {
          'msvs_precompiled_source': '../common/precompile.cc',
          'msvs_precompiled_header': '../common/precompile.h',
        },
      },
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
        '../common/common.gyp:ie_common_settings',
        '../common/common.gyp:ie_guids',
        '../plugin/toolband/toolband.gyp:toolband_idl',
        '../../../base/base.gyp:base',
        '../../../breakpad/breakpad.gyp:breakpad_handler',
        '../../../ceee/common/common.gyp:ceee_common',
        '<(DEPTH)/chrome/chrome.gyp:chrome_version_header',
        '<(DEPTH)/chrome_frame/chrome_frame.gyp:chrome_frame_ie',  # for GUIDs.
      ],
      'msvs_settings': {
        'VCLinkerTool': {
          'OutputFile': '$(OutDir)/servers/$(ProjectName).exe',
          # Set /SUBSYSTEM:WINDOWS since this is not a command-line program.
          'SubSystem': '2',
        },
      },
      'libraries': [
        'oleacc.lib',
        'iepmapi.lib',
      ],
    },
  ]
}
