# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'android_webview_test_apk',
      'type': 'none',
      'dependencies': [
        '../base/base.gyp:base_java_test_support',
        '../content/content.gyp:content_java_test_support',
        '../net/net.gyp:net_java_test_support',
       'android_webview_java',
        'libwebviewchromium',
      ],
      'variables': {
        'apk_name': 'AndroidWebViewTest',
        'java_in_dir': '../android_webview/javatests',
        'resource_dir': 'res',
        'is_test_apk': 1,
        'additional_input_paths': [
          '<(PRODUCT_DIR)/android_webview_test_apk/assets/asset_file.html',
          '<(PRODUCT_DIR)/android_webview_test_apk/assets/asset_icon.png',
        ],
      },
      'copies': [
        {
          'destination': '<(PRODUCT_DIR)/android_webview_test_apk/assets',
          'files': [
            '<(java_in_dir)/assets/asset_file.html',
            '<(java_in_dir)/assets/asset_icon.png',
          ],
        },
      ],
      'includes': [ '../build/java_apk.gypi' ],
    },
    {
      'target_name': 'android_webview_unittests',
      'type': '<(gtest_target_type)',
      'dependencies': [
        '../base/base.gyp:test_support_base',
        '../net/net.gyp:net_test_support',
        '../testing/android/native_test.gyp:native_test_native_code',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        'android_webview_common',
      ],
      'include_dirs': [
        '..',
        '../skia/config',
        '<(SHARED_INTERMEDIATE_DIR)/android_webview_unittests',
      ],
      'sources': [
        'browser/net/android_stream_reader_url_request_job_unittest.cc',
        'browser/net/input_stream_reader_unittest.cc',
        'lib/main/webview_tests.cc',
        'native/input_stream_unittest.cc',
        'native/state_serializer_unittests.cc',
      ],
    },
    {
      'target_name': 'android_webview_unittest_java',
      'type': 'none',
      'dependencies': [
        '../base/base.gyp:base_java_test_support',
        '../content/content.gyp:content_java_test_support',
        'android_webview_java',
      ],
      'variables': {
        'java_in_dir': '../android_webview/unittestjava',
      },
      'includes': [ '../build/java.gypi' ],
    },
    {
      'target_name': 'android_webview_unittests_jni',
      'type': 'none',
      'sources': [
          '../android_webview/unittestjava/src/org/chromium/android_webview/unittest/InputStreamUnittest.java',
      ],
      'variables': {
        'jni_gen_dir': 'android_webview_unittests',
      },
      'includes': [ '../build/jni_generator.gypi' ],
    },
    {
      'target_name': 'android_webview_unittests_apk',
      'type': 'none',
      'dependencies': [
        'android_webview_unittest_java',
        'android_webview_unittests',
        'android_webview_unittests_jni',
      ],
      'variables': {
        'test_suite_name': 'android_webview_unittests',
        'input_shlib_path': '<(SHARED_LIB_DIR)/<(SHARED_LIB_PREFIX)android_webview_unittests<(SHARED_LIB_SUFFIX)',
      },
      'includes': [ '../build/apk_test.gypi' ],
    },
  ],
}
