# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'variables': {
    'chromium_code': 1,
   },
  'targets': [
    {
      'target_name': 'ios_unittests',
      'type': '<(gtest_target_type)',
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:run_all_unittests',
        '../base/base.gyp:test_support_base',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        'ios_base.gyp:ios_consumer_base',
        'web/ios_web.gyp:ios_web',
      ],
      'sources': [
        'consumer/base/supports_user_data_unittest.cc',
        'web/navigation/navigation_item_impl_unittest.mm',
      ],
    },
  ],
}
