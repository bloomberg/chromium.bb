# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'conditions': [
    ['OS=="android"', {
      'targets': [
        {
          'target_name': 'native_test_apk',
          'message': 'building native test package',
          'type': 'none',
          'actions': [
            {
              'action_name': 'native_test_apk',
              'inputs': [
                '<(DEPTH)/base/test/android/native_test_apk.xml',
                '<!@(find test/android/java -name "*.java")',
                'native_test_launcher.cc'
              ],
              'outputs': [
                # Awkwardly, we build a Debug APK even when gyp is in
                # Release mode.  I don't think it matters (e.g. we're
                # probably happy to not codesign) but naming should be
                # fixed.
                '<(PRODUCT_DIR)/ChromeNativeTests-debug.apk',
              ],
              'action': [
                'ant',
                '-DPRODUCT_DIR=<(PRODUCT_DIR)',
                '-buildfile',
                '<(DEPTH)/base/test/android/native_test_apk.xml',
              ]
            }
          ],
        },
      ],
    }]
  ]
}
