// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/android/platform_jni_loader.h"

namespace chromecast {
namespace android {

bool PlatformRegisterJni(JNIEnv* env) {
  // Intentional no-op for public build.
  return true;
}

}  // namespace android
}  // namespace chromecast
