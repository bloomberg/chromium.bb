// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BATTERY_STATUS_BATTERY_STATUS_MANAGER_ANDROID_H_
#define CHROME_BROWSER_BATTERY_STATUS_BATTERY_STATUS_MANAGER_ANDROID_H_

#include "base/android/scoped_java_ref.h"
#include "content/common/content_export.h"

namespace content {

// Android implementation of Battery Status API.
class CONTENT_EXPORT BatteryStatusManagerAndroid {
 public:
  // Must be called at startup.
  static bool Register(JNIEnv* env);

  // Called from Java via JNI.
  void GotBatteryStatus(JNIEnv*, jobject, jboolean charging,
                        jdouble chargingTime, jdouble dischargingTime,
                        jdouble level);

  bool StartListeningBatteryChange();
  void StopListeningBatteryChange();

 protected:
  BatteryStatusManagerAndroid();
  virtual ~BatteryStatusManagerAndroid();

 private:
  // Java provider of battery status info.
  base::android::ScopedJavaGlobalRef<jobject> j_manager_;

  DISALLOW_COPY_AND_ASSIGN(BatteryStatusManagerAndroid);
};

}  // namespace content

#endif  // CHROME_BROWSER_BATTERY_STATUS_BATTERY_STATUS_MANAGER_ANDROID_H_
