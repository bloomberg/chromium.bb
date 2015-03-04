// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_APP_ANDROID_CHROME_JNI_ONLOAD_H_
#define CHROME_APP_ANDROID_CHROME_JNI_ONLOAD_H_

#include "base/android/base_jni_onload.h"

namespace chrome {
namespace android {

bool OnJNIOnLoadRegisterJNI(
    JavaVM* vm,
    base::android::RegisterCallback callback);

bool OnJNIOnLoadInit(base::android::InitCallback callback);

}  // namespace android
}  // namespace chrome

#endif  // CHROME_APP_ANDROID_CHROME_JNI_ONLOAD_H_
