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
        'web/ios_web.gyp:test_support_ios_web',
      ],
      'sources': [
        'web/browser_state_unittest.cc',
        'web/navigation/navigation_item_impl_unittest.mm',
        'web/public/webp_decoder_unittest.mm',
        'web/string_util_unittest.cc',
        'web/url_scheme_util_unittest.mm',
      ],
      'actions': [
        {
          'action_name': 'copy_test_data',
          'variables': {
            'test_data_files': [
              'web/test/data/test.jpg',
              'web/test/data/test.webp',
              'web/test/data/test_alpha.webp',
              'web/test/data/test_alpha.png',
              'web/test/data/test_small.tiff',
              'web/test/data/test_small.webp',
            ],
            'test_data_prefix': 'ios',
          },
          'includes': [ '../build/copy_test_data_ios.gypi' ],
        },
      ],
    },
  ],
}
