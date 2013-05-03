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
            'auto_login_parser/auto_login_parser_unittest.cc',
            'navigation_interception/intercept_navigation_resource_throttle_unittest.cc',
            'sessions/serialized_navigation_entry_unittest.cc',
            'test/run_all_unittests.cc',
            'visitedlink/test/visitedlink_unittest.cc',
            'webdata/encryptor/encryptor_password_mac_unittest.cc',
            'webdata/encryptor/encryptor_unittest.cc',
          ],
          'include_dirs': [
            '..',
          ],
          'dependencies': [
            '../base/base.gyp:test_support_base',
            '../testing/gmock.gyp:gmock',
            '../testing/gtest.gyp:gtest',

            # Dependencies of auto_login_parser
            'auto_login_parser',

            # Dependencies of encryptor
            'encryptor',

            # Dependencies of intercept_navigation_resource_throttle_unittest.cc
            '../content/content.gyp:test_support_content',
            '../skia/skia.gyp:skia',
            'navigation_interception',

            # Dependencies of sessions
            '../third_party/protobuf/protobuf.gyp:protobuf_lite',
            'sessions',
            'sessions_test_support',

            # Dependencies of visitedlink
            'visitedlink_browser',
            'visitedlink_renderer',
            '../content/content_resources.gyp:content_resources',
          ],
          'conditions': [
            ['OS == "android" and gtest_target_type == "shared_library"', {
              'dependencies': [
                '../testing/android/native_test.gyp:native_test_native_code',
              ]
            }],
            ['OS=="win" and win_use_allocator_shim==1', {
              'dependencies': [
                '../base/allocator/allocator.gyp:allocator',
              ],
            }],
            ['android_webview_build == 0', {
              'dependencies': [
                '../sync/sync.gyp:sync',
              ],
            }],
          ],
          # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
          'msvs_disabled_warnings': [4267, ],
        },
        {
          'target_name': 'components_perftests',
          'type': '<(gtest_target_type)',
          'dependencies': [
            '../base/base.gyp:base',
            '../base/base.gyp:test_support_perf',
            '../content/content.gyp:test_support_content',
            '../testing/gtest.gyp:gtest',
            '../ui/compositor/compositor.gyp:compositor',
            'visitedlink_browser',
          ],
         'include_dirs': [
           '..',
         ],
         'sources': [
           'visitedlink/test/visitedlink_perftest.cc',
         ],
         'conditions': [
           ['OS == "android" and gtest_target_type == "shared_library"', {
             'dependencies': [
               '../testing/android/native_test.gyp:native_test_native_code',
             ],
           }],
         ],
         # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
         'msvs_disabled_warnings': [ 4267, ],
        },
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
