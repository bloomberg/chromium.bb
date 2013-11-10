// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_ANDROID_VIEW_CONFIGURATION_H_
#define CONTENT_COMMON_ANDROID_VIEW_CONFIGURATION_H_

#include <jni.h>

namespace content {

// Provides access to Android's ViewConfiguration for gesture-related constants.
class ViewConfiguration {
 public:
  static int GetDoubleTapTimeoutInMs();
  static int GetLongPressTimeoutInMs();
  static int GetTapTimeoutInMs();

  static int GetMaximumFlingVelocityInDipsPerSecond();
  static int GetMinimumFlingVelocityInDipsPerSecond();

  static int GetMaximumFlingVelocityInPixelsPerSecond();
  static int GetMinimumFlingVelocityInPixelsPerSecond();

  // Registers methods with JNI and returns true if succeeded.
  static bool RegisterViewConfiguration(JNIEnv* env);
};

}  // namespace content

#endif  // CONTENT_COMMON_ANDROID_VIEW_CONFIGURATION_H_
