# Copyright (c) 2012 The Chromium Authors. All rights reserved.
#
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'conditions': [
    ['OS=="android"', {
      'targets': [
        {
          'target_name': 'web_contents_delegate_android',
          'type': 'static_library',
          'dependencies': [
            '../base/base.gyp:base',
            '../net/net.gyp:net',
            '../skia/skia.gyp:skia',
            '../ui/ui.gyp:ui',
            '../webkit/support/webkit_support.gyp:glue',
            'content_browser',
            'content_common',
            'web_contents_delegate_android_jni_headers',
          ],
          'include_dirs': [
            '..',
            '../skia/config',
            '<(SHARED_INTERMEDIATE_DIR)/web_contents_delegate_android',
          ],
          'sources': [
            'components/web_contents_delegate_android/color_chooser_android.cc',
            'components/web_contents_delegate_android/color_chooser_android.h',
            'components/web_contents_delegate_android/component_jni_registrar.cc',
            'components/web_contents_delegate_android/component_jni_registrar.h',
            'components/web_contents_delegate_android/web_contents_delegate_android.cc',
            'components/web_contents_delegate_android/web_contents_delegate_android.h',
          ],
        },
        {
          'target_name': 'web_contents_delegate_android_java',
          'type': 'none',
          'dependencies': [
            '../base/base.gyp:base',
            'content_java',
          ],
          'variables': {
            'package_name': 'web_contents_delegate_android',
            'java_in_dir': 'components/web_contents_delegate_android/java',
          },
          'includes': [ '../build/java.gypi' ],
        },
        {
          'target_name': 'web_contents_delegate_android_jni_headers',
          'type': 'none',
          'sources': [
            'components/web_contents_delegate_android/java/src/org/chromium/content/components/web_contents_delegate_android/ColorChooserAndroid.java',
            'components/web_contents_delegate_android/java/src/org/chromium/content/components/web_contents_delegate_android/WebContentsDelegateAndroid.java',
          ],
          'variables': {
            'jni_gen_dir': 'web_contents_delegate_android',
          },
          'includes': [ '../build/jni_generator.gypi' ],
        },
      ],
    }],
  ],
}
