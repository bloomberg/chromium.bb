# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'conditions': [
    ['OS != "ios"', {
      'targets': [
        {
          'target_name': 'components_unittests',
          'type': '<(gtest_target_type)',
          'sources': [
            'navigation_interception/intercept_navigation_resource_throttle_unittest.cc',
            'test/run_all_unittests.cc',
          ],
          'include_dirs': [
            '..',
          ],
          'dependencies': [
            '../base/base.gyp:test_support_base',
            '../testing/gmock.gyp:gmock',
            '../testing/gtest.gyp:gtest',

            # Dependencies of intercept_navigation_resource_throttle_unittest.cc
            '../content/content.gyp:test_support_content',
            '../skia/skia.gyp:skia',
            'navigation_interception',
          ],
          'conditions': [
            ['OS == "android" and gtest_target_type == "shared_library"', {
              'dependencies': [
                '../testing/android/native_test.gyp:native_test_native_code',
              ]
            }],
          ],
        }
      ],
      'conditions': [
        ['OS == "android" and gtest_target_type == "shared_library"', {
          'targets': [
            {
              'target_name': 'components_unittests_apk',
              'type': 'none',
              'dependencies': [
                'components_unittests',
              ],
              'variables': {
                'test_suite_name': 'components_unittests',
                'input_shlib_path': '<(SHARED_LIB_DIR)/<(SHARED_LIB_PREFIX)components_unittests<(SHARED_LIB_SUFFIX)',
              },
              'includes': [ '../build/apk_test.gypi' ],
            },
          ],
        }],
      ],
    }],
  ],
}
