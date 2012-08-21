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
        'aw_web_contents_delegate.cc',
        'aw_web_contents_delegate.h',
      ],
    },
    {
      'target_name': 'android_webview_native_jni',
      'type': 'none',
      'sources': [
      ],
      'variables': {
        'jni_gen_dir': 'android_webview',
      },
      'includes': [ '../../build/jni_generator.gypi' ],
    },
  ],
}
