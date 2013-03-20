# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'android_webview_test_java',
      'type': 'none',
      'dependencies': [
        '../base/base.gyp:base_java_test_support',
        '../content/content.gyp:content_java_test_support',
        '../net/net.gyp:net_java_test_support',
        'android_webview_java',
        'libwebviewchromium',
      ],
      'variables': {
        'java_in_dir': '../android_webview/javatests',
        'has_java_resources': 1,
        'R_package': 'org.chromium.android_webview.test',
        'R_package_relpath': 'org/chromium/android_webview/test',
      },
      'includes': [ '../build/java.gypi' ],
    },
    {
      'target_name': 'android_webview_test_apk',
      'type': 'none',
      'dependencies': [
        'android_webview_test_java',
      ],
      'variables': {
        'apk_name': 'AndroidWebViewTest',
        'java_in_dir': '../android_webview/test_apk',
        'is_test_apk': 0, # We want resources from android_webview_test_java.
        'additional_input_paths': [
          '<(PRODUCT_DIR)/android_webview_test_apk/assets/asset_file.html',
          '<(PRODUCT_DIR)/android_webview_test_apk/assets/asset_icon.png',
          '<(PRODUCT_DIR)/android_webview_test_apk/assets/full_screen_video_test.html',
        ],
      },
      'copies': [
        {
          'destination': '<(PRODUCT_DIR)/android_webview_test_apk/assets',
          'files': [
            '<(java_in_dir)/assets/asset_file.html',
            '<(java_in_dir)/assets/asset_icon.png',
            '<(java_in_dir)/assets/full_screen_video_test.html',
          ],
        },
      ],
      'includes': [ '../build/java_apk.gypi' ],
    },
    {
      'target_name': 'android_webview_shell_apk',
      'type': 'none',
      'dependencies': [
        'android_webview_test_java',
        'android_webview_pak',
      ],
      'variables': {
        'apk_name': 'AndroidWebViewShell',
        'java_in_dir': '../android_webview/shell_apk',
        'native_libs_paths': ['<(SHARED_LIB_DIR)/libwebviewchromium.so'],
        'resource_dir': 'res',
        'asset_location': '<(ant_build_out)/android_webview_apk/assets',
        'additional_input_paths': [
          '<(PRODUCT_DIR)/android_webview_apk/assets/webviewchromium.pak',
        ],
      },
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
        'jni_gen_package': 'android_webview_unittests',
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
