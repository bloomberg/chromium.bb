# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'webview_native',
      'type': 'static_library',
      'dependencies': [
        '../../base/base.gyp:base_static',
        '../../chrome/browser/component/components.gyp:web_contents_delegate_android',
        'android_webview_native_jni',
      ],
      'include_dirs': [
        '../..',
        '../../skia/config',
        '<(SHARED_INTERMEDIATE_DIR)/android_webview',
      ],
      'sources': [
        'android_webview_jni_registrar.cc',
        'android_webview_jni_registrar.h',
        'android_web_view_util.cc',
        'android_web_view_util.h',
        'aw_http_auth_handler.cc',
        'aw_http_auth_handler.h',
        'aw_browser_dependency_factory.cc',
        'aw_browser_dependency_factory.h',
        'aw_contents_container.h',
        'aw_contents.cc',
        'aw_contents.h',
        'aw_web_contents_delegate.cc',
        'aw_web_contents_delegate.h',
        'cookie_manager.cc',
        'cookie_manager.h',
        'intercepted_request_data.cc',
        'intercepted_request_data.h',
      ],
    },
    {
      'target_name': 'android_webview_native_jni',
      'type': 'none',
      'sources': [
          '../java/src/org/chromium/android_webview/AndroidWebViewUtil.java',
          '../java/src/org/chromium/android_webview/AwContents.java',
          '../java/src/org/chromium/android_webview/AwHttpAuthHandler.java',
          '../java/src/org/chromium/android_webview/CookieManager.java',
          '../java/src/org/chromium/android_webview/InterceptedRequestData.java',
      ],
      'variables': {
        'jni_gen_dir': 'android_webview',
      },
      'includes': [ '../../build/jni_generator.gypi' ],
    },
  ],
}
