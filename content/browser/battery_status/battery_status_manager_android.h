// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "content/browser/battery_status/battery_status_manager.h"

#ifndef CHROME_BROWSER_BATTERY_STATUS_BATTERY_STATUS_MANAGER_ANDROID_H_
#define CHROME_BROWSER_BATTERY_STATUS_BATTERY_STATUS_MANAGER_ANDROID_H_

namespace content {

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
  virtual bool StartListeningBatteryChange() OVERRIDE;
  virtual void StopListeningBatteryChange() OVERRIDE;

  BatteryStatusService::BatteryUpdateCallback callback_;
  // Java provider of battery status info.
  base::android::ScopedJavaGlobalRef<jobject> j_manager_;

  DISALLOW_COPY_AND_ASSIGN(BatteryStatusManagerAndroid);
};

}  // namespace content

#endif  // CHROME_BROWSER_BATTERY_STATUS_BATTERY_STATUS_MANAGER_ANDROID_H_
