// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/battery_status/battery_status_manager_android.h"

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/metrics/histogram.h"
#include "jni/BatteryStatusManager_jni.h"

using base::android::AttachCurrentThread;

namespace content {

BatteryStatusManagerAndroid::BatteryStatusManagerAndroid(
    const BatteryStatusService::BatteryUpdateCallback& callback)
    : callback_(callback) {
  j_manager_.Reset(
      Java_BatteryStatusManager_getInstance(
          AttachCurrentThread(), base::android::GetApplicationContext()));
}

BatteryStatusManagerAndroid::~BatteryStatusManagerAndroid() {
}

// static
bool BatteryStatusManagerAndroid::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

void BatteryStatusManagerAndroid::GotBatteryStatus(JNIEnv*,
                                                   jobject,
                                                   jboolean charging,
                                                   jdouble charging_time,
                                                   jdouble discharging_time,
                                                   jdouble level) {
  blink::WebBatteryStatus status;
  status.charging = charging;
  status.chargingTime = charging_time;
  status.dischargingTime = discharging_time;
  status.level = level;
  callback_.Run(status);
}

bool BatteryStatusManagerAndroid::StartListeningBatteryChange() {
  bool result = Java_BatteryStatusManager_start(AttachCurrentThread(),
      j_manager_.obj(), reinterpret_cast<intptr_t>(this));
  UMA_HISTOGRAM_BOOLEAN("BatteryStatus.StartAndroid", result);
  return result;
}

void BatteryStatusManagerAndroid::StopListeningBatteryChange() {
  Java_BatteryStatusManager_stop(
      AttachCurrentThread(), j_manager_.obj());
}

// static
scoped_ptr<BatteryStatusManager> BatteryStatusManager::Create(
    const BatteryStatusService::BatteryUpdateCallback& callback) {
  return scoped_ptr<BatteryStatusManager>(
      new BatteryStatusManagerAndroid(callback));
}

}  // namespace content
