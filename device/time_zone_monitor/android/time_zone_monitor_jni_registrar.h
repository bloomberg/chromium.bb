// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_TIME_ZONE_MONITOR_ANDROID_TIME_ZONE_MONITOR_JNI_REGISTRAR_H_
#define DEVICE_TIME_ZONE_MONITOR_ANDROID_TIME_ZONE_MONITOR_JNI_REGISTRAR_H_

#include <jni.h>

#include "device/time_zone_monitor/time_zone_monitor_export.h"

namespace device {
namespace android {

bool DEVICE_TIME_ZONE_MONITOR_EXPORT RegisterTimeZoneMonitorJni(JNIEnv* env);

}  // namespace android
}  // namespace device

#endif  // DEVICE_TIME_ZONE_MONITOR_ANDROID_TIME_ZONE_MONITOR_JNI_REGISTRAR_H_
