// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr/register_jni.h"

#include "chrome/browser/android/vr/jni_registration.h"

#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "base/android/jni_utils.h"
#include "base/stl_util.h"

#include "third_party/gvr-android-sdk/display_synchronizer_jni.h"
#include "third_party/gvr-android-sdk/gvr_api_jni.h"
#include "third_party/gvr-android-sdk/native_callbacks_jni.h"

namespace vr {

// These VR native functions are not handled by the automatic registration, so
// they are manually registered here.
static const base::android::RegistrationMethod kGvrRegisteredMethods[] = {
    {"DisplaySynchronizer",
     DisplaySynchronizer::RegisterDisplaySynchronizerNatives},
    {"GvrApi", GvrApi::RegisterGvrApiNatives},
    {"NativeCallbacks", NativeCallbacks::RegisterNativeCallbacksNatives},
};

bool RegisterJni(JNIEnv* env) {
  if (!base::android::IsSelectiveJniRegistrationEnabled(env)) {
    if (!vr::RegisterNonMainDexNatives(env) ||
        !RegisterNativeMethods(env, kGvrRegisteredMethods,
                               base::size(kGvrRegisteredMethods))) {
      return false;
    }
  }
  if (!vr::RegisterMainDexNatives(env)) {
    return false;
  }
  return true;
}

}  // namespace vr
