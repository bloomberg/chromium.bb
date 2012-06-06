// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_DEVICE_INFO_H_
#define CONTENT_BROWSER_ANDROID_DEVICE_INFO_H_
#pragma once

#include <jni.h>
#include <string>

#include "base/android/scoped_java_ref.h"
#include "base/basictypes.h"

namespace content {

// Facilitates access to device information typically only
// available using the Android SDK, including Display properties.
class DeviceInfo {
 public:
  DeviceInfo();
  ~DeviceInfo();

  int GetHeight();
  int GetWidth();
  int GetBitsPerPixel();
  int GetBitsPerComponent();
  double GetDPIScale();
  double GetRefreshRate();
  std::string GetNetworkCountryIso();

 private:
  base::android::ScopedJavaGlobalRef<jobject> j_device_info_;

  DISALLOW_COPY_AND_ASSIGN(DeviceInfo);
};

bool RegisterDeviceInfo(JNIEnv* env);

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_DEVICE_INFO_H_
