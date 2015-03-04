# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


{
  'targets': [
   {
      # GN Version: //components/data_reduction_proxy/content
      'target_name': 'data_reduction_proxy_content',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../content/content.gyp:content_browser',
        '../skia/skia.gyp:skia',
        '../ui/base/ui_base.gyp:ui_base',
        'components_resources.gyp:components_resources',
        'components_strings.gyp:components_strings',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'data_reduction_proxy/content/browser/content_data_reduction_proxy_debug_ui_service.cc',
        'data_reduction_proxy/content/browser/content_data_reduction_proxy_debug_ui_service.h',
        'data_reduction_proxy/content/browser/data_reduction_proxy_debug_blocking_page.cc',
        'data_reduction_proxy/content/browser/data_reduction_proxy_debug_blocking_page.h',
        'data_reduction_proxy/content/browser/data_reduction_proxy_debug_resource_throttle.cc',
        'data_reduction_proxy/content/browser/data_reduction_proxy_debug_resource_throttle.h',
        'data_reduction_proxy/content/browser/data_reduction_proxy_debug_ui_manager.cc',
        'data_reduction_proxy/content/browser/data_reduction_proxy_debug_ui_manager.h',
      ],
    },
    {
      # GN version: //components/data_reduction_proxy/content/common
      'target_name': 'data_reduction_proxy_content_common',
      'type': 'static_library',
      'dependencies': [
        '../content/content.gyp:content_common',
        '../ipc/ipc.gyp:ipc',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'data_reduction_proxy/content/common/data_reduction_proxy_messages.cc',
        'data_reduction_proxy/content/common/data_reduction_proxy_messages.h',
      ],
    },
    {
      # GN version: //components/data_reduction_proxy/content/browser
      'target_name': 'data_reduction_proxy_content_browser',
      'type': 'static_library',
      'dependencies': [
        '../content/content.gyp:content_common',
        '../ipc/ipc.gyp:ipc',
        'data_reduction_proxy_content_common',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'data_reduction_proxy/content/browser/data_reduction_proxy_message_filter.cc',
        'data_reduction_proxy/content/browser/data_reduction_proxy_message_filter.h',
      ],
    },
    {
      # GN version: //components/data_reduction_proxy/core/browser
      'target_name': 'data_reduction_proxy_core_browser',
      'type': 'static_library',
      'conditions': [
        ['OS != "android" and OS != "ios"', {
          'dependencies' : [
            '../google_apis/google_apis.gyp:google_apis',
          ]
        }],
      ],
      'dependencies': [
        'data_reduction_proxy_version_header',
        '../base/base.gyp:base',
        '../crypto/crypto.gyp:crypto',
        '../net/net.gyp:net',
        '../url/url.gyp:url_lib',
        'data_reduction_proxy_core_common',
        'pref_registry',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        "data_reduction_proxy/core/browser/data_reduction_proxy_debug_ui_service.h",
        'data_reduction_proxy/core/browser/data_reduction_proxy_bypass_protocol.cc',
        'data_reduction_proxy/core/browser/data_reduction_proxy_bypass_protocol.h',
        'data_reduction_proxy/core/browser/data_reduction_proxy_config.cc',
        'data_reduction_proxy/core/browser/data_reduction_proxy_config.h',
        'data_reduction_proxy/core/browser/data_reduction_proxy_configurator.cc',
        'data_reduction_proxy/core/browser/data_reduction_proxy_configurator.h',
        'data_reduction_proxy/core/browser/data_reduction_proxy_delegate.cc',
        'data_reduction_proxy/core/browser/data_reduction_proxy_delegate.h',
        'data_reduction_proxy/core/browser/data_reduction_proxy_interceptor.cc',
        'data_reduction_proxy/core/browser/data_reduction_proxy_interceptor.h',
        'data_reduction_proxy/core/browser/data_reduction_proxy_io_data.cc',
        'data_reduction_proxy/core/browser/data_reduction_proxy_io_data.h',
        'data_reduction_proxy/core/browser/data_reduction_proxy_metrics.cc',
        'data_reduction_proxy/core/browser/data_reduction_proxy_metrics.h',
        'data_reduction_proxy/core/browser/data_reduction_proxy_network_delegate.cc',
        'data_reduction_proxy/core/browser/data_reduction_proxy_network_delegate.h',
        'data_reduction_proxy/core/browser/data_reduction_proxy_prefs.cc',
        'data_reduction_proxy/core/browser/data_reduction_proxy_prefs.h',
        'data_reduction_proxy/core/browser/data_reduction_proxy_request_options.cc',
        'data_reduction_proxy/core/browser/data_reduction_proxy_request_options.h',
        'data_reduction_proxy/core/browser/data_reduction_proxy_service.cc',
        'data_reduction_proxy/core/browser/data_reduction_proxy_service.h',
        'data_reduction_proxy/core/browser/data_reduction_proxy_settings.cc',
        'data_reduction_proxy/core/browser/data_reduction_proxy_settings.h',
        'data_reduction_proxy/core/browser/data_reduction_proxy_statistics_prefs.cc',
        'data_reduction_proxy/core/browser/data_reduction_proxy_statistics_prefs.h',
        'data_reduction_proxy/core/browser/data_reduction_proxy_tamper_detection.cc',
        'data_reduction_proxy/core/browser/data_reduction_proxy_tamper_detection.h',
        'data_reduction_proxy/core/browser/data_reduction_proxy_usage_stats.cc',
        'data_reduction_proxy/core/browser/data_reduction_proxy_usage_stats.h',
      ],
    },
    {
      # GN version: //components/data_reduction_proxy/core/common
      'target_name': 'data_reduction_proxy_core_common',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../url/url.gyp:url_lib',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'data_reduction_proxy/core/common/data_reduction_proxy_bypass_type_list.h',
        'data_reduction_proxy/core/common/data_reduction_proxy_event_store.cc',
        'data_reduction_proxy/core/common/data_reduction_proxy_event_store.h',
        'data_reduction_proxy/core/common/data_reduction_proxy_headers.cc',
        'data_reduction_proxy/core/common/data_reduction_proxy_headers.h',
        'data_reduction_proxy/core/common/data_reduction_proxy_params.cc',
        'data_reduction_proxy/core/common/data_reduction_proxy_params.h',
        'data_reduction_proxy/core/common/data_reduction_proxy_pref_names.cc',
        'data_reduction_proxy/core/common/data_reduction_proxy_pref_names.h',
        'data_reduction_proxy/core/common/data_reduction_proxy_switches.cc',
        'data_reduction_proxy/core/common/data_reduction_proxy_switches.h',
      ],
    },
    {
      # GN version: //components/data_reduction_proxy/core/browser:test_support
      'target_name': 'data_reduction_proxy_test_support',
      'type': 'static_library',
      'dependencies' : [
        '../base/base.gyp:base',
        '../net/net.gyp:net',
        '../net/net.gyp:net_test_support',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        'data_reduction_proxy_content',
        'data_reduction_proxy_core_browser',
        'data_reduction_proxy_core_common',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'data_reduction_proxy/core/browser/data_reduction_proxy_config_test_utils.cc',
        'data_reduction_proxy/core/browser/data_reduction_proxy_config_test_utils.h',
        'data_reduction_proxy/core/browser/data_reduction_proxy_configurator_test_utils.cc',
        'data_reduction_proxy/core/browser/data_reduction_proxy_configurator_test_utils.h',
        'data_reduction_proxy/core/browser/data_reduction_proxy_settings_test_utils.cc',
        'data_reduction_proxy/core/browser/data_reduction_proxy_settings_test_utils.h',
        'data_reduction_proxy/core/browser/data_reduction_proxy_test_utils.cc',
        'data_reduction_proxy/core/browser/data_reduction_proxy_test_utils.h',
        'data_reduction_proxy/core/common/data_reduction_proxy_headers_test_utils.cc',
        'data_reduction_proxy/core/common/data_reduction_proxy_headers_test_utils.h',
        'data_reduction_proxy/core/common/data_reduction_proxy_params_test_utils.cc',
        'data_reduction_proxy/core/common/data_reduction_proxy_params_test_utils.h',
      ],
      # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
      'msvs_disabled_warnings': [4267, ],
    },
    {
      'target_name': 'data_reduction_proxy_version_header',
      'type': 'none',
      'direct_dependent_settings': {
        'include_dirs': [
          '<(SHARED_INTERMEDIATE_DIR)',
        ],
      },
      'actions': [
        {
          'action_name': 'version_header',
          'message': 'Generating version header file: <@(_outputs)',
          'inputs': [
            '<(version_path)',
            'data_reduction_proxy/core/common/version.h.in',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/components/data_reduction_proxy/core/common/version.h',
          ],
          'action': [
            'python',
            '<(version_py_path)',
            '-e', 'VERSION_FULL="<(version_full)"',
            'data_reduction_proxy/core/common/version.h.in',
            '<@(_outputs)',
          ],
          'includes': [
            '../build/util/version.gypi',
          ],
        },
      ],
    },
  ],
}
