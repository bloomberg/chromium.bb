// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/gcm_driver_android.h"

#include "base/compiler_specific.h"

namespace gcm {
static void Java_GCMDriver_doNothing(JNIEnv* env) ALLOW_UNUSED;
}  // namespace gcm

// Must come after the ALLOW_UNUSED declaration.
#include "jni/GCMDriver_jni.h"

namespace gcm {

// static
bool GCMDriverAndroid::RegisterBindings(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace gcm