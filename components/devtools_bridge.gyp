# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'devtools_bridge_javalib',
      'type': 'none',
      'variables': {
        'java_in_dir': 'devtools_bridge/android/java',
      },
      'includes': [ '../build/java.gypi' ],
    },
    {
      'target_name': 'devtools_bridge_testutils',
      'type': 'none',
      'variables': {
        'java_in_dir': 'devtools_bridge/test/android/javatests',
      },
      'includes': [ '../build/java.gypi' ],
      'dependencies': [
        'devtools_bridge_javalib',
      ],
    },
    {
      'target_name': 'devtools_bridge_tests_apk',
      'type': 'none',
      'dependencies': [
        'devtools_bridge_javalib',
        'devtools_bridge_testutils',
      ],
      'variables': {
        'apk_name': 'DevToolsBridgeTest',
        'test_suite_name': 'devtools_bridge_tests',
        'java_in_dir': 'devtools_bridge/android/javatests',
        'is_test_apk': 1,
      },
      'includes': [ '../build/java_apk.gypi' ],
    },
  ],
}
