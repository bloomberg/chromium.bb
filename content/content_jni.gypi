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
      'public/android/java/org/chromium/content/browser/CommandLine.java',
      'public/android/java/org/chromium/content/browser/DeviceOrientation.java',
      'public/android/java/org/chromium/content/browser/JNIHelper.java',
      'public/android/java/org/chromium/content/browser/LibraryLoader.java',
      'public/android/java/org/chromium/content/browser/LocationProvider.java',
      'public/android/java/org/chromium/content/browser/TraceEvent.java',
    ],
    'jni_headers': [
      '<(SHARED_INTERMEDIATE_DIR)/content/jni/command_line_jni.h',
      '<(SHARED_INTERMEDIATE_DIR)/content/jni/device_orientation_jni.h',
      '<(SHARED_INTERMEDIATE_DIR)/content/jni/jni_helper_jni.h',
      '<(SHARED_INTERMEDIATE_DIR)/content/jni/library_loader_jni.h',
      '<(SHARED_INTERMEDIATE_DIR)/content/jni/location_provider_jni.h',
      '<(SHARED_INTERMEDIATE_DIR)/content/jni/trace_event_jni.h',
    ],
  },
  'includes': [ '../build/jni_generator.gypi' ],
}
