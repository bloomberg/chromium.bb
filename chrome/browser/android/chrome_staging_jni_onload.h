// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_CHROME_STAGING_JNI_ONLOAD_H_
#define CHROME_BROWSER_ANDROID_CHROME_STAGING_JNI_ONLOAD_H_

#include <jni.h>

namespace chrome {
namespace android {

bool OnJNIOnLoadRegisterJNI(JavaVM* vm);

bool OnJNIOnLoadInit();

}  // namespace android
}  // namespace chrome

#endif  // CHROME_BROWSER_ANDROID_CHROME_STAGING_JNI_ONLOAD_H_
