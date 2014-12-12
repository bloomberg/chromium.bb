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
        '../../base/base.gyp:run_all_unittests',
        '../../base/base.gyp:test_support_base',
        '../../net/net.gyp:net_test_support',
        '../../testing/gtest.gyp:gtest',
        '../web/ios_web.gyp:ios_web',
        '../web/ios_web.gyp:test_support_ios_web',
        'ios_chrome.gyp:ios_chrome_browser',
      ],
      'sources': [
        'browser/net/image_fetcher_unittest.mm',
      ],
    },
  ],
}
