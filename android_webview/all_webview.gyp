# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This is used as the top-level gyp file for building WebView in the Android
# tree. It should depend only on native code, as we cannot currently generate
# correct makefiles to build Java code via gyp in the Android tree.

{
  'targets': [
    {
      'target_name': 'All',
      'type': 'none',
      'dependencies': [
        'android_webview.gyp:libwebviewchromium',

        # Needed by android_webview_java
        '../base/base.gyp:base_java_activity_state',
        '../content/content.gyp:page_transition_types_java',
        '../content/content.gyp:result_codes_java',
        '../net/net.gyp:certificate_mime_types_java',
        '../net/net.gyp:cert_verify_result_android_java',
        '../net/net.gyp:net_errors_java',
        '../net/net.gyp:private_key_types_java',
      ],
    }, # target_name: All
  ],  # targets
}
