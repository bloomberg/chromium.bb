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
        # Generated java files from templates should then be listed in android_webview/Android.mk
        '../base/base.gyp:base_java_application_state',
        '../base/base.gyp:base_java_memory_pressure_level_list',
        '../content/content.gyp:gesture_event_type_java',
        '../content/content.gyp:page_transition_types_java',
        '../content/content.gyp:popup_item_type_java',
        '../content/content.gyp:result_codes_java',
        '../content/content.gyp:screen_orientation_values_java',
        '../content/content.gyp:speech_recognition_error_java',
        '../media/media.gyp:media_android_imageformat_list',
        '../net/net.gyp:certificate_mime_types_java',
        '../net/net.gyp:cert_verify_status_android_java',
        '../net/net.gyp:net_errors_java',
        '../net/net.gyp:private_key_types_java',
        '../ui/android/ui_android.gyp:window_open_disposition_java',
        '../ui/android/ui_android.gyp:bitmap_format_java',
      ],
    }, # target_name: All
  ],  # targets
}
