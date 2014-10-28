// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "device/battery/battery_status_manager.h"

#ifndef DEVICE_BATTERY_BATTERY_STATUS_MANAGER_ANDROID_H_
#define DEVICE_BATTERY_BATTERY_STATUS_MANAGER_ANDROID_H_

namespace device {

class BatteryStatusManagerAndroid : public BatteryStatusManager {
 public:
  explicit BatteryStatusManagerAndroid(
      const BatteryStatusService::BatteryUpdateCallback& callback);
  virtual ~BatteryStatusManagerAndroid();

  // Must be called at startup.
  static bool Register(JNIEnv* env);

  // Called from Java via JNI.
  void GotBatteryStatus(JNIEnv*,
                        jobject,
                        jboolean charging,
                        jdouble charging_time,
                        jdouble discharging_time,
                        jdouble level);

 private:
  // BatteryStatusManager:
  virtual bool StartListeningBatteryChange() override;
  virtual void StopListeningBatteryChange() override;

  BatteryStatusService::BatteryUpdateCallback callback_;
  // Java provider of battery status info.
  base::android::ScopedJavaGlobalRef<jobject> j_manager_;

  DISALLOW_COPY_AND_ASSIGN(BatteryStatusManagerAndroid);
};

}  // namespace device

#endif  // DEVICE_BATTERY_BATTERY_STATUS_MANAGER_ANDROID_H_
