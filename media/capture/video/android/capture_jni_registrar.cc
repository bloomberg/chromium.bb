// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/android/capture_jni_registrar.h"

#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "base/macros.h"
#include "media/capture/video/android/video_capture_device_android.h"

namespace media {

static base::android::RegistrationMethod kCaptureRegisteredMethods[] = {
    {"VideoCaptureDevice",
     VideoCaptureDeviceAndroid::RegisterVideoCaptureDevice},
};

bool RegisterCaptureJni(JNIEnv* env) {
  return base::android::RegisterNativeMethods(
      env, kCaptureRegisteredMethods, arraysize(kCaptureRegisteredMethods));
}

}  // namespace media
