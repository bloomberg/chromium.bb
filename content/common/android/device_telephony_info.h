// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_ANDROID_DEVICE_TELEPHONY_INFO_H_
#define CONTENT_COMMON_ANDROID_DEVICE_TELEPHONY_INFO_H_

#include <jni.h>
#include <string>

#include "base/android/scoped_java_ref.h"
#include "base/basictypes.h"

namespace content {

// Facilitates access to device information typically only
// available using the Android SDK, including Telephony properties.
class DeviceTelephonyInfo {
 public:
  DeviceTelephonyInfo();
  ~DeviceTelephonyInfo();

  // Returns the ISO country code equivalent of the current MCC.
  std::string GetNetworkCountryIso();

  // Registers methods with JNI and returns true if succeeded.
  static bool RegisterDeviceTelephonyInfo(JNIEnv* env);

 private:
  base::android::ScopedJavaGlobalRef<jobject> j_device_info_;

  DISALLOW_COPY_AND_ASSIGN(DeviceTelephonyInfo);
};

}  // namespace content

#endif  // CONTENT_COMMON_ANDROID_DEVICE_TELEPHONY_INFO_H_
