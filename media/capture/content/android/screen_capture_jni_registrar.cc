// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/content/android/screen_capture_jni_registrar.h"

#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "base/macros.h"
#include "media/capture/content/android/screen_capture_machine_android.h"

namespace media {

static const base::android::RegistrationMethod kCaptureRegisteredMethods[] = {
    {"ScreenCaptureMachine",
     ScreenCaptureMachineAndroid::RegisterScreenCaptureMachine},
};

bool RegisterScreenCaptureJni(JNIEnv* env) {
  return base::android::RegisterNativeMethods(
      env, kCaptureRegisteredMethods, arraysize(kCaptureRegisteredMethods));
}

}  // namespace media
