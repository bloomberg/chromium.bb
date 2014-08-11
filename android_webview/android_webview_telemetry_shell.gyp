# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'android_webview_telemetry_shell_apk',
      'type': 'none',
      'variables': {
        'apk_name': 'AndroidWebViewTelemetryShell',
        'java_in_dir': 'tools/WebViewTelemetryShell',
        'resource_dir': 'tools/WebViewTelemetryShell/res',
      },
      'includes': [ '../build/java_apk.gypi' ],
    },
  ],
}
