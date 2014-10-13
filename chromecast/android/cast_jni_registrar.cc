// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/android/cast_jni_registrar.h"

#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "chromecast/browser/android/cast_window_android.h"
#include "chromecast/browser/android/cast_window_manager.h"
#include "chromecast/browser/android/external_video_surface_container_impl.h"
#include "chromecast/crash/android/crash_handler.h"

namespace chromecast {
namespace android {

namespace {

static base::android::RegistrationMethod kMethods[] = {
  { "CastWindowAndroid", shell::CastWindowAndroid::RegisterJni },
  { "CastWindowManager", shell::RegisterCastWindowManager },
  { "CrashHandler", CrashHandler::RegisterCastCrashJni },
  { "ExternalVideoSurfaceContainer",
        shell::RegisterExternalVideoSurfaceContainer },
};

}  // namespace

bool RegisterJni(JNIEnv* env) {
  return RegisterNativeMethods(env, kMethods, arraysize(kMethods));
}

}  // namespace android
}  // namespace chromecast
