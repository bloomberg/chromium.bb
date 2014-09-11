// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_ANDROID_PLATFORM_JNI_LOADER_H_
#define CHROMECAST_ANDROID_PLATFORM_JNI_LOADER_H_

#include <jni.h>

namespace chromecast {
namespace android {

bool PlatformRegisterJni(JNIEnv* env);

}  // namespace android
}  // namespace chromecast

#endif  // CHROMECAST_ANDROID_PLATFORM_JNI_LOADER_H_
