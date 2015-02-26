# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'type': 'shared_library',
  'android_unmangled_name': 1,
  'dependencies': [
    'android_webview_common',
  ],
  'variables': {
    'use_native_jni_exports': 1,
  },
  'conditions': [
    [ 'android_webview_build==1', {
      'dependencies': [
        # When building inside the android tree we also need to depend on all
        # the java sources generated from templates which will be needed by
        # android_webview_java in android_webview/java_library_common.mk.
        '../base/base.gyp:base_java_application_state',
        '../base/base.gyp:base_java_library_load_from_apk_status_codes',
        '../base/base.gyp:base_java_library_process_type',
        '../base/base.gyp:base_java_memory_pressure_level',
        '../content/content.gyp:console_message_level_java',
        '../content/content.gyp:content_gamepad_mapping',
        '../content/content.gyp:gesture_event_type_java',
        '../content/content.gyp:invalidate_types_java',
        '../content/content.gyp:navigation_controller_java',
        '../content/content.gyp:popup_item_type_java',
        '../content/content.gyp:result_codes_java',
        '../content/content.gyp:screen_orientation_values_java',
        '../content/content.gyp:speech_recognition_error_java',
        '../content/content.gyp:top_controls_state_java',
        '../media/media.gyp:media_android_imageformat',
        '../net/net.gyp:cert_verify_status_android_java',
        '../net/net.gyp:certificate_mime_types_java',
        '../net/net.gyp:network_change_notifier_types_java',
        '../net/net.gyp:net_errors_java',
        '../net/net.gyp:private_key_types_java',
        '../third_party/WebKit/public/blink_headers.gyp:web_input_event_java',
        '../third_party/WebKit/public/blink_headers.gyp:web_text_input_type',
        '../ui/android/ui_android.gyp:android_resource_type_java',
        '../ui/android/ui_android.gyp:bitmap_format_java',
        '../ui/android/ui_android.gyp:page_transition_types_java',
        '../ui/android/ui_android.gyp:system_ui_resource_type_java',
        '../ui/android/ui_android.gyp:touch_device_types_java',
        '../ui/android/ui_android.gyp:window_open_disposition_java',
        '../ui/android/ui_android.gyp:text_input_type_java',
        '../ui/touch_selection/ui_touch_selection.gyp:selection_event_type_java',
        # We also need to depend on the Java bindings generated from the .mojom files.
        '../device/battery/battery.gyp:device_battery_mojo_bindings_for_webview',
      ],
      # Enable feedback-directed optimisation for the library when building in
      # android.
      'aosp_build_settings': {
        'LOCAL_FDO_SUPPORT': 'true',
      },
    }],
  ],
  'sources': [
    'lib/main/webview_entry_point.cc',
  ],
}
