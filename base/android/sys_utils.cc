// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/sys_utils.h"

#include "base/sys_info.h"
#include "jni/SysUtils_jni.h"

// Any device that reports a physical RAM size less than this, in megabytes
// is considered 'low-end'. IMPORTANT: Read the LinkerLowMemoryThresholdTest
// comments in build/android/pylib/linker/test_case.py before modifying this
// value.
#define ANDROID_LOW_MEMORY_DEVICE_THRESHOLD_MB 512

const int64 kLowEndMemoryThreshold =
    1024 * 1024 * ANDROID_LOW_MEMORY_DEVICE_THRESHOLD_MB;

// Defined and called by JNI
static jboolean IsLowEndDevice(JNIEnv* env, jclass clazz) {
  return base::android::SysUtils::IsLowEndDevice();
}

namespace base {
namespace android {

bool SysUtils::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

bool SysUtils::IsLowEndDevice() {
  return SysInfo::AmountOfPhysicalMemory() <= kLowEndMemoryThreshold;
}

SysUtils::SysUtils() { }

}  // namespace android
}  // namespace base
