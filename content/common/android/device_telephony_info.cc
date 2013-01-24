// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/android/device_telephony_info.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/logging.h"
#include "jni/DeviceTelephonyInfo_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::ScopedJavaLocalRef;

namespace content {

DeviceTelephonyInfo::DeviceTelephonyInfo() {
  JNIEnv* env = AttachCurrentThread();
  j_device_info_.Reset(Java_DeviceTelephonyInfo_create(env,
      base::android::GetApplicationContext()));
}

DeviceTelephonyInfo::~DeviceTelephonyInfo() {
}

std::string DeviceTelephonyInfo::GetNetworkCountryIso() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> result =
      Java_DeviceTelephonyInfo_getNetworkCountryIso(env, j_device_info_.obj());
  return ConvertJavaStringToUTF8(result);
}

// static
bool DeviceTelephonyInfo::RegisterDeviceTelephonyInfo(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace content
