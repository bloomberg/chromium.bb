# Copyright (c) 2012 The Chromium Authors. All rights reserved.
#
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    'navigation_interception/navigation_interception.gypi',
    'web_contents_delegate_android/web_contents_delegate_android.gypi',
  ],

  'conditions': [
    ['OS=="android"', {
      'targets': [
        {
          'target_name': 'browser_component_jni_headers',
          'type': 'none',
          'dependencies': [
              # TODO(jknotten) Remove this dependency once external deps have
              # been updated to refer directly to the below.
              '../../../content/content.gyp:navigation_interception_jni_headers'
           ],
          'sources': [
            'web_contents_delegate_android/java/src/org/chromium/chrome/browser/component/web_contents_delegate_android/WebContentsDelegateAndroid.java',
          ],
          'variables': {
            'jni_gen_dir': 'chrome/browser_component',
          },
          'includes': [ '../../../build/jni_generator.gypi' ],
        },
      ],
    }],
  ],
}
