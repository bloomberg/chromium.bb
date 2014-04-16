// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/battery_status/battery_status_manager_android.h"

#include <string.h>

#include "base/android/jni_android.h"
#include "jni/BatteryStatusManager_jni.h"

using base::android::AttachCurrentThread;

namespace content {

BatteryStatusManagerAndroid::BatteryStatusManagerAndroid() {
  j_manager_.Reset(
      Java_BatteryStatusManager_getInstance(
          AttachCurrentThread(), base::android::GetApplicationContext()));
}

BatteryStatusManagerAndroid::~BatteryStatusManagerAndroid() {
  StopListeningBatteryChange();
}

bool BatteryStatusManagerAndroid::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

void BatteryStatusManagerAndroid::GotBatteryStatus(JNIEnv*, jobject,
    jboolean charging, jdouble chargingTime, jdouble dischargingTime,
    jdouble level) {
  NOTIMPLEMENTED();
}

bool BatteryStatusManagerAndroid::StartListeningBatteryChange() {
  return Java_BatteryStatusManager_start(
      AttachCurrentThread(), j_manager_.obj(),
      reinterpret_cast<intptr_t>(this));
}

void BatteryStatusManagerAndroid::StopListeningBatteryChange() {
  Java_BatteryStatusManager_stop(
      AttachCurrentThread(), j_manager_.obj());
}

}  // namespace content
