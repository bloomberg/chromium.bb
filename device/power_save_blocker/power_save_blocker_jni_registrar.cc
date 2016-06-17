// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/power_save_blocker/power_save_blocker_jni_registrar.h"

#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "base/macros.h"
#include "device/power_save_blocker/power_save_blocker_android.h"

namespace device {
namespace android {
namespace {

const base::android::RegistrationMethod kRegisteredMethods[] = {
    {"PowerSaveBlock", device::RegisterPowerSaveBlocker},
};

}  // namespace

bool RegisterPowerSaveBlockerJni(JNIEnv* env) {
  return RegisterNativeMethods(env, kRegisteredMethods,
                               arraysize(kRegisteredMethods));
}

}  // namespace android
}  // namespace device
