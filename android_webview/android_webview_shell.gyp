# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'android_webview_shell_apk',
      'type': 'none',
      'dependencies': [
        '../base/base.gyp:base_java_test_support',
      ],
      'variables': {
        'apk_name': 'AndroidWebViewShell',
        'java_in_dir': 'tools/WebViewShell',
        'resource_dir': 'tools/WebViewShell/res',
        'is_test_apk': 1,
        'test_type': 'instrumentation',
        'isolate_file': 'android_webview_shell_test_apk.isolate',
      },
      'includes': [
        '../build/java_apk.gypi',
        '../build/android/test_runner.gypi',
      ],
    },
  ],
}
