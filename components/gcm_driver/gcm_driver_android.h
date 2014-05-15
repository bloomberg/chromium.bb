// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GCM_DRIVER_GCM_DRIVER_ANDROID_H
#define COMPONENTS_GCM_DRIVER_GCM_DRIVER_ANDROID_H

#include <jni.h>

namespace gcm {

class GCMDriverAndroid {
 public:
  // Register JNI methods
  static bool RegisterBindings(JNIEnv* env);
};

}  // namespace gcm

#endif  // COMPONENTS_GCM_DRIVER_GCM_DRIVER_ANDROID_H