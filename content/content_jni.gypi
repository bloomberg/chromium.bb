# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  # TODO(jrg): Update this action and other jni generators to only
  # require specifying the java directory and generate the rest.
  # TODO(jrg): when doing the above, make sure we support multiple
  # output directories (e.g. browser/jni and common/jni if needed).
  'variables': {
    'java_sources': [
      'public/android/java/org/chromium/content/app/ContentMain.java',
      'public/android/java/org/chromium/content/app/LibraryLoader.java',
      'public/android/java/org/chromium/content/app/SandboxedProcessService.java',
      'public/android/java/org/chromium/content/app/UserAgent.java',
      'public/android/java/org/chromium/content/browser/AndroidBrowserProcess.java',
      'public/android/java/org/chromium/content/browser/ContentView.java',
      'public/android/java/org/chromium/content/browser/ContentViewClient.java',
      'public/android/java/org/chromium/content/browser/DeviceInfo.java',
      'public/android/java/org/chromium/content/browser/DeviceOrientation.java',
      'public/android/java/org/chromium/content/browser/DownloadController.java',
      'public/android/java/org/chromium/content/browser/LocationProvider.java',
      'public/android/java/org/chromium/content/browser/SandboxedProcessLauncher.java',
      'public/android/java/org/chromium/content/browser/TouchPoint.java',
      'public/android/java/org/chromium/content/common/CommandLine.java',
      'public/android/java/org/chromium/content/common/SurfaceCallback.java',
      'public/android/java/org/chromium/content/common/TraceEvent.java',
    ],
    'jni_headers': [
      '<(SHARED_INTERMEDIATE_DIR)/content/jni/content_main_jni.h',
      '<(SHARED_INTERMEDIATE_DIR)/content/jni/library_loader_jni.h',
      '<(SHARED_INTERMEDIATE_DIR)/content/jni/sandboxed_process_service_jni.h',
      '<(SHARED_INTERMEDIATE_DIR)/content/jni/user_agent_jni.h',
      '<(SHARED_INTERMEDIATE_DIR)/content/jni/android_browser_process_jni.h',
      '<(SHARED_INTERMEDIATE_DIR)/content/jni/content_view_jni.h',
      '<(SHARED_INTERMEDIATE_DIR)/content/jni/content_view_client_jni.h',
      '<(SHARED_INTERMEDIATE_DIR)/content/jni/device_info_jni.h',
      '<(SHARED_INTERMEDIATE_DIR)/content/jni/device_orientation_jni.h',
      '<(SHARED_INTERMEDIATE_DIR)/content/jni/download_controller_jni.h',
      '<(SHARED_INTERMEDIATE_DIR)/content/jni/location_provider_jni.h',
      '<(SHARED_INTERMEDIATE_DIR)/content/jni/sandboxed_process_launcher_jni.h',
      '<(SHARED_INTERMEDIATE_DIR)/content/jni/touch_point_jni.h',
      '<(SHARED_INTERMEDIATE_DIR)/content/jni/command_line_jni.h',
      '<(SHARED_INTERMEDIATE_DIR)/content/jni/surface_callback_jni.h',
      '<(SHARED_INTERMEDIATE_DIR)/content/jni/trace_event_jni.h',
    ],
  },
  'includes': [ '../build/jni_generator.gypi' ],
}
