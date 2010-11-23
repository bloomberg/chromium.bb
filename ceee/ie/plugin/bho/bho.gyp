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
      'target_name': 'bho',
      'type': 'static_library',
      'dependencies': [
        '../toolband/toolband.gyp:toolband_idl',
        '../../broker/broker.gyp:broker',
        '../../broker/broker.gyp:broker_rpc_idl',
        '../../broker/broker.gyp:broker_rpc_lib',
        '../../common/common.gyp:ie_common',
        '../../common/common.gyp:ie_common_settings',
        '../../../common/common.gyp:ceee_common',
        '../../../common/common.gyp:initializing_coclass',
        '../../../../base/base.gyp:base',
        '../../../../chrome_frame/chrome_frame.gyp:chrome_tab_idl',
        # For the vtable patching stuff...
        '<(DEPTH)/chrome_frame/chrome_frame.gyp:chrome_frame_ie',
      ],
      'sources': [
        'browser_helper_object.cc',
        'browser_helper_object.h',
        'browser_helper_object.rgs',
        'cookie_accountant.cc',
        'cookie_accountant.h',
        'cookie_events_funnel.cc',
        'cookie_events_funnel.h',
        'dom_utils.cc',
        'dom_utils.h',
        'events_funnel.cc',
        'events_funnel.h',
        'executor.cc',
        'executor.h',
        'extension_port_manager.cc',
        'extension_port_manager.h',
        'frame_event_handler.cc',
        'frame_event_handler.h',
        'http_negotiate.cc',
        'http_negotiate.h',
        'infobar_browser_window.cc',
        'infobar_browser_window.h',
        'infobar_events_funnel.cc',
        'infobar_events_funnel.h',
        'infobar_manager.cc',
        'infobar_manager.h',
        'infobar_window.cc',
        'infobar_window.h',
        'tab_events_funnel.cc',
        'tab_events_funnel.h',
        'tab_window_manager.cc',
        'tab_window_manager.h',
        'tool_band_visibility.cc',
        'tool_band_visibility.h',
        'web_browser_events_source.h',
        'webnavigation_events_funnel.cc',
        'webnavigation_events_funnel.h',
        'webrequest_events_funnel.cc',
        'webrequest_events_funnel.h',
        'webrequest_notifier.cc',
        'webrequest_notifier.h',
        'web_progress_notifier.cc',
        'web_progress_notifier.h',
        'window_message_source.cc',
        'window_message_source.h',

        '../../../../chrome_frame/renderer_glue.cc',  # needed for cf_ie.lib
        '../../../../chrome/common/extensions/extension_resource.cc',
        '../../../../chrome/common/extensions/extension_resource.h',
      ],
      'include_dirs': [
        # For chrome_tab.h
        '<(SHARED_INTERMEDIATE_DIR)',
      ],
      'libraries': [
        'rpcrt4.lib',
      ],
    },
  ]
}
