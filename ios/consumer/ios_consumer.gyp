# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'variables': {
    'chromium_code': 1,
   },
  'targets': [
    {
      'target_name': 'ios_consumer_base',
      'type': 'static_library',
      'dependencies': [
        '../../base/base.gyp:base',
      ],
      'include_dirs': [
        '../..',
      ],
      'sources': [
        'base/supports_user_data.cc',
        'base/util.mm',
        'public/base/supports_user_data.h',
        'public/base/util.h',
      ],
    },
    {
      'target_name': 'ios_consumer_unittests',
      'type': '<(gtest_target_type)',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../base/base.gyp:run_all_unittests',
        '../../base/base.gyp:test_support_base',
        '../../testing/gmock.gyp:gmock',
        '../../testing/gtest.gyp:gtest',
        'ios_consumer_base',
      ],
      'sources': [
        'base/supports_user_data_unittest.cc',
      ],
    },
  ],
}
