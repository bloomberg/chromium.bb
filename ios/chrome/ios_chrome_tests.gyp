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
        '../../components/components.gyp:bookmarks_test_support',
        '../../components/components.gyp:enhanced_bookmarks_test_support',
        '../../net/net.gyp:net_test_support',
        '../../testing/gmock.gyp:gmock',
        '../../testing/gtest.gyp:gtest',
        '../../third_party/ocmock/ocmock.gyp:ocmock',
        '../ios_tests.gyp:test_support_ios',
        '../web/ios_web.gyp:ios_web',
        '../web/ios_web.gyp:test_support_ios_web',
        'ios_chrome.gyp:ios_chrome_app',
        'ios_chrome.gyp:ios_chrome_browser',
        'ios_chrome.gyp:ios_chrome_common',
        'ios_chrome_test_support',
      ],
      'sources': [
        'app/safe_mode_util_unittest.cc',
        'browser/chrome_url_util_unittest.mm',
        'browser/crash_loop_detection_util_unittest.mm',
        'browser/enhanced_bookmarks/bookmark_image_service_ios_unittest.mm',
        'browser/experimental_flags_unittest.mm',
        'browser/geolocation/CLLocation+XGeoHeaderTest.mm',
        'browser/geolocation/location_manager_unittest.mm',
        'browser/install_time_util_unittest.mm',
        'browser/installation_notifier_unittest.mm',
        'browser/memory/memory_wedge_unittest.cc',
        'browser/net/image_fetcher_unittest.mm',
        'browser/net/metrics_network_client_unittest.mm',
        'browser/net/retryable_url_fetcher_unittest.mm',
        'browser/snapshots/snapshot_cache_unittest.mm',
        'browser/snapshots/snapshots_util_unittest.mm',
        'browser/translate/translate_service_ios_unittest.cc',
        'browser/ui/commands/set_up_for_testing_command_unittest.mm',
        'browser/ui/ui_util_unittest.mm',
        'browser/ui/uikit_ui_util_unittest.mm',
        'common/string_util_unittest.mm',
      ],
      'actions': [
        {
          'action_name': 'copy_ios_chrome_test_data',
          'variables': {
            'test_data_files': [
              'test/data/webdata/bookmarkimages',
            ],
            'test_data_prefix': 'ios/chrome',
          },
          'includes': [ '../../build/copy_test_data_ios.gypi' ]
        },
      ],
      'includes': ['ios_chrome_resources_bundle.gypi'],
    },
    {
      'target_name': 'ios_chrome_test_support',
      'type': 'static_library',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../testing/gmock.gyp:gmock',
        '../../testing/gtest.gyp:gtest',
        '../../ui/base/ui_base.gyp:ui_base',
        '../../url/url.gyp:url_lib',
        '../provider/ios_provider_chrome.gyp:ios_provider_chrome_browser',
        'ios_chrome.gyp:ios_chrome_browser',
      ],
      'sources': [
        'browser/geolocation/location_manager+Testing.h',
        'browser/geolocation/test_location_manager.h',
        'browser/geolocation/test_location_manager.mm',
        'browser/net/mock_image_fetcher.h',
        'browser/net/mock_image_fetcher.mm',
        'browser/sync/sync_setup_service_mock.cc',
        'browser/sync/sync_setup_service_mock.h',
        'test/ios_chrome_unit_test_suite.cc',
        'test/ios_chrome_unit_test_suite.h',
        'test/run_all_unittests.cc',
        'test/testing_application_context.cc',
        'test/testing_application_context.h',
      ],
    },
  ],
}
