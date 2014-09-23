# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


{
  'targets': [
    {
      # GN version: //components/data_reduction_proxy/browser
      'target_name': 'data_reduction_proxy_browser',
      'type': 'static_library',
      'dependencies': [
        'data_reduction_proxy_version_header',
        '../base/base.gyp:base',
        '../crypto/crypto.gyp:crypto',
        '../net/net.gyp:net',
        'data_reduction_proxy_common',
        'pref_registry',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'data_reduction_proxy/browser/data_reduction_proxy_auth_request_handler.cc',
        'data_reduction_proxy/browser/data_reduction_proxy_auth_request_handler.h',
        'data_reduction_proxy/browser/data_reduction_proxy_config_service.cc',
        'data_reduction_proxy/browser/data_reduction_proxy_config_service.h',
        'data_reduction_proxy/browser/data_reduction_proxy_configurator.h',
        'data_reduction_proxy/browser/data_reduction_proxy_delegate.cc',
        'data_reduction_proxy/browser/data_reduction_proxy_delegate.h',
        'data_reduction_proxy/browser/data_reduction_proxy_metrics.cc',
        'data_reduction_proxy/browser/data_reduction_proxy_metrics.h',
        'data_reduction_proxy/browser/data_reduction_proxy_params.cc',
        'data_reduction_proxy/browser/data_reduction_proxy_params.h',
        'data_reduction_proxy/browser/data_reduction_proxy_prefs.cc',
        'data_reduction_proxy/browser/data_reduction_proxy_prefs.h',
        'data_reduction_proxy/browser/data_reduction_proxy_protocol.cc',
        'data_reduction_proxy/browser/data_reduction_proxy_protocol.h',
        'data_reduction_proxy/browser/data_reduction_proxy_settings.cc',
        'data_reduction_proxy/browser/data_reduction_proxy_settings.h',
        'data_reduction_proxy/browser/data_reduction_proxy_statistics_prefs.cc',
        'data_reduction_proxy/browser/data_reduction_proxy_statistics_prefs.h',
        'data_reduction_proxy/browser/data_reduction_proxy_tamper_detection.cc',
        'data_reduction_proxy/browser/data_reduction_proxy_tamper_detection.h',
        'data_reduction_proxy/browser/data_reduction_proxy_usage_stats.cc',
        'data_reduction_proxy/browser/data_reduction_proxy_usage_stats.h',
      ],
    },
    {
      # GN version: //components/data_reduction_proxy/common
      'target_name': 'data_reduction_proxy_common',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'data_reduction_proxy/common/data_reduction_proxy_headers.cc',
        'data_reduction_proxy/common/data_reduction_proxy_headers.h',
        'data_reduction_proxy/common/data_reduction_proxy_pref_names.cc',
        'data_reduction_proxy/common/data_reduction_proxy_pref_names.h',
        'data_reduction_proxy/common/data_reduction_proxy_switches.cc',
        'data_reduction_proxy/common/data_reduction_proxy_switches.h',
      ],
    },
    {
      # GN version: //components/data_reduction_proxy/browser:test_support
      'target_name': 'data_reduction_proxy_test_support',
      'type': 'static_library',
      'dependencies' : [
        '../base/base.gyp:base',
        '../net/net.gyp:net',
        '../net/net.gyp:net_test_support',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        'data_reduction_proxy_browser',
        'data_reduction_proxy_common',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'data_reduction_proxy/browser/data_reduction_proxy_params_test_utils.cc',
        'data_reduction_proxy/browser/data_reduction_proxy_params_test_utils.h',
        'data_reduction_proxy/browser/data_reduction_proxy_settings_test_utils.cc',
        'data_reduction_proxy/browser/data_reduction_proxy_settings_test_utils.h',
        'data_reduction_proxy/common/data_reduction_proxy_headers_test_utils.cc',
        'data_reduction_proxy/common/data_reduction_proxy_headers_test_utils.h',
      ],
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
            'data_reduction_proxy/common/version.h.in',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/components/data_reduction_proxy/common/version.h',
          ],
          'action': [
            'python',
            '<(version_py_path)',
            '-e', 'VERSION_FULL="<(version_full)"',
            'data_reduction_proxy/common/version.h.in',
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

