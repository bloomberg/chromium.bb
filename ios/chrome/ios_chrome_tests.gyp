# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'ios_chrome_unittests',
      'type': '<(gtest_target_type)',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../base/base.gyp:test_support_base',
        '../../net/net.gyp:net_test_support',
        '../../testing/gmock.gyp:gmock',
        '../../testing/gtest.gyp:gtest',
        '../../third_party/ocmock/ocmock.gyp:ocmock',
        '../ios_tests.gyp:test_support_ios',
        '../web/ios_web.gyp:ios_web',
        '../web/ios_web.gyp:test_support_ios_web',
        'ios_chrome.gyp:ios_chrome_browser',
        'ios_chrome_test_support',
      ],
      'sources': [
        'browser/net/image_fetcher_unittest.mm',
        'browser/snapshots/snapshot_cache_unittest.mm',
        'browser/snapshots/snapshots_util_unittest.mm',
        'browser/translate/translate_service_ios_unittest.cc',
        'browser/ui/commands/set_up_for_testing_command_unittest.mm',
        'browser/ui/ui_util_unittest.mm',
        'browser/ui/uikit_ui_util_unittest.mm',
      ],
    },
    {
      'target_name': 'ios_chrome_test_support',
      'type': 'static_library',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../testing/gmock.gyp:gmock',
        '../../testing/gtest.gyp:gtest',
        '../provider/ios_provider_chrome.gyp:ios_provider_chrome_browser',
        'ios_chrome.gyp:ios_chrome_browser',
      ],
      'sources': [
        'browser/net/mock_image_fetcher.h',
        'browser/net/mock_image_fetcher.mm',
        'test/ios_chrome_unit_test_suite.cc',
        'test/ios_chrome_unit_test_suite.h',
        'test/run_all_unittests.cc',
        'test/testing_application_context.cc',
        'test/testing_application_context.h',
      ],
    },
  ],
}
