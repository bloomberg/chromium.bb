# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'ios_web_unittests',
      'type': '<(gtest_target_type)',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../base/base.gyp:run_all_unittests',
        '../../base/base.gyp:test_support_base',
        '../../testing/gmock.gyp:gmock',
        '../../testing/gtest.gyp:gtest',
        '../../third_party/ocmock/ocmock.gyp:ocmock',
        '../testing/ios_testing.gyp:ocmock_support',
        'ios_web.gyp:ios_web',
        'ios_web.gyp:test_support_ios_web',
      ],
      'sources': [
        'alloc_with_zone_interceptor_unittest.mm',
        'browser_state_unittest.cc',
        'crw_network_activity_indicator_manager_unittest.mm',
        'history_state_util_unittest.mm',
        'navigation/navigation_item_impl_unittest.mm',
        'navigation/nscoder_util_unittest.mm',
        'net/cert_policy_unittest.cc',
        'net/clients/crw_js_injection_network_client_unittest.mm',
        'net/clients/crw_passkit_network_client_unittest.mm',
        'net/request_group_util_unittest.mm',
        'net/request_tracker_impl_unittest.mm',
        'net/web_http_protocol_handler_delegate_unittest.mm',
        'public/referrer_util_unittest.cc',
        'string_util_unittest.cc',
        'ui_web_view_util_unittest.mm',
        'url_scheme_util_unittest.mm',
        'url_util_unittest.cc',
        'weak_nsobject_counter_unittest.mm',
        'web_state/ui/crw_static_file_web_view_unittest.mm',
      ],
      'actions': [
        {
          'action_name': 'copy_test_data',
          'variables': {
            'test_data_files': [
              'test/data/chrome.html',
              'test/data/testbadpass.pkpass',
              'test/data/testpass.pkpass',
            ],
            'test_data_prefix': 'ios/web',
          },
          'includes': [ '../../build/copy_test_data_ios.gypi' ],
        },
      ],
    },
  ],
}
