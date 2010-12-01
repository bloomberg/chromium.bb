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
      'target_name': 'ie_common_settings',
      'type': 'none',
      'direct_dependent_settings': {
        'defines': [
          # TODO(joi@chromium.org) Put into an include somewhere.
          '_WIN32_WINDOWS=0x0410',
          '_WIN32_IE=0x0600',
          '_ATL_CSTRING_EXPLICIT_CONSTRUCTORS',
          '_ATL_STATIC_REGISTRY',
          '_WTL_NO_CSTRING',
        ],
        'include_dirs': [
          '../../../third_party/wtl/include',
        ],
      },
    },
    {
      'target_name': 'ie_guids',
      'type': 'static_library',
      'dependencies': [
        'ie_common_settings',
        '../plugin/toolband/toolband.gyp:toolband_idl',
        '<(DEPTH)/chrome_frame/chrome_frame.gyp:chrome_tab_idl',
      ],
      'sources': [
        'ie_guids.cc',
      ],
      'include_dirs': [
        '../../..',
        # For chrome_tab.h
        '<(SHARED_INTERMEDIATE_DIR)',
      ],
    },
    {
      'target_name': 'ie_common',
      'type': 'static_library',
      'dependencies': [
        'ie_common_settings',
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/breakpad/breakpad.gyp:breakpad_handler',
        '<(DEPTH)/build/temp_gyp/googleurl.gyp:googleurl',
        '<(DEPTH)/ceee/common/common.gyp:initializing_coclass',
        '<(DEPTH)/ceee/common/common.gyp:ceee_common',
        '<(DEPTH)/ceee/ie/plugin/toolband/toolband.gyp:toolband_idl',
        '<(DEPTH)/chrome/chrome.gyp:chrome_version_header',
        '<(DEPTH)/chrome_frame/chrome_frame.gyp:chrome_tab_idl',
        '<(DEPTH)/net/net.gyp:net_base',
      ],
      'sources': [
        'api_registration.h',
        'ceee_util.cc',
        'ceee_util.h',
        'chrome_frame_host.cc',
        'chrome_frame_host.h',
        'constants.cc',
        'constants.h',
        'crash_reporter.cc',
        'crash_reporter.h',
        'extension_manifest.cc',
        'extension_manifest.h',
        'ie_tab_interfaces.cc',
        'ie_tab_interfaces.h',
        'ie_util.cc',
        'ie_util.h',
        'mock_ie_tab_interfaces.h',
        'ceee_module_util.cc',
        'ceee_module_util.h',
        'metrics_util.h',

        # TODO(joi@chromium.org) Refactor to use chrome/common library.
        '../../../chrome/browser/automation/extension_automation_constants.cc',
        '../../../chrome/browser/extensions/'
            'extension_bookmarks_module_constants.cc',
        '../../../chrome/browser/extensions/extension_event_names.cc',
        '../../../chrome/browser/extensions/'
            'extension_page_actions_module_constants.cc',
        '../../../chrome/browser/extensions/extension_cookies_api_constants.cc',
        '../../../chrome/browser/extensions/'
            'extension_infobar_module_constants.cc',
        '../../../chrome/browser/extensions/extension_tabs_module_constants.cc',
        '../../../chrome/browser/extensions/'
            'extension_webnavigation_api_constants.cc',
        '../../../chrome/browser/extensions/'
            'extension_webrequest_api_constants.cc',
        '../../../chrome/common/chrome_switches.cc',
        '../../../chrome/common/chrome_switches.h',
        '../../../chrome/common/url_constants.cc',
        '../../../chrome/common/url_constants.h',
        '../../../chrome/common/extensions/extension_constants.cc',
        '../../../chrome/common/extensions/extension_constants.h',
        '../../../chrome/common/extensions/extension_error_utils.cc',
        '../../../chrome/common/extensions/extension_error_utils.h',
        '../../../chrome/common/extensions/url_pattern.cc',
        '../../../chrome/common/extensions/url_pattern.h',
        '../../../chrome/common/extensions/user_script.cc',
        '../../../chrome/common/extensions/user_script.h',
      ],
      'include_dirs': [
        # For chrome_tab.h and version.h
        '<(SHARED_INTERMEDIATE_DIR)',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          # Because we use some of the chrome files above directly, we need
          # to specify thess include paths which they depend on.
          '../../../skia/config/win',
          '../../../third_party/skia/include/config',
        ],
      },
    },
  ]
}
