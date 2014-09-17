// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/android/cast_jni_registrar.h"

#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "chromecast/shell/browser/android/cast_window_android.h"
#include "chromecast/shell/browser/android/cast_window_manager.h"

namespace chromecast {
namespace android {

namespace {

static base::android::RegistrationMethod kMethods[] = {
  { "CastWindowAndroid", shell::CastWindowAndroid::RegisterJni },
  { "CastWindowManager", shell::RegisterCastWindowManager },
};

}  // namespace

bool RegisterJni(JNIEnv* env) {
  return RegisterNativeMethods(env, kMethods, arraysize(kMethods));
}

}  // namespace android
}  // namespace chromecast
