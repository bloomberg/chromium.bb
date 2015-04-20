# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'android_webview_shell_apk',
      'type': 'none',
      'variables': {
        'apk_name': 'AndroidWebViewShell',
        'java_in_dir': 'tools/WebViewShell',
        'resource_dir': 'tools/WebViewShell/res',
      },
      'includes': [ '../build/java_apk.gypi' ],
    },
  ],
}
