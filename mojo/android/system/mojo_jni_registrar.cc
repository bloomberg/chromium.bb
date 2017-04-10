// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/android/system/mojo_jni_registrar.h"

#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "mojo/android/system/core_impl.h"
#include "mojo/android/system/watcher_impl.h"

namespace mojo {
namespace android {

static base::android::RegistrationMethod kMojoRegisteredMethods[] = {
    {"CoreImpl", mojo::android::RegisterCoreImpl},
    {"WatcherImpl", mojo::android::RegisterWatcherImpl},
};

bool RegisterSystemJni(JNIEnv* env) {
  return base::android::RegisterNativeMethods(
      env, kMojoRegisteredMethods, arraysize(kMojoRegisteredMethods));
}

}  // namespace android
}  // namespace mojo
