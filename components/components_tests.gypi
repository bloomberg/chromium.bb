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
        }
      ],
    }],
  ],
}
