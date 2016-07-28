// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/android/sync_jni_registrar.h"

#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "base/macros.h"
#include "components/sync/android/model_type_helper.h"

namespace syncer {

base::android::RegistrationMethod kSyncRegisteredMethods[] = {
    {"ModelTypeHelper", RegisterModelTypeHelperJni},
};

bool RegisterSyncJni(JNIEnv* env) {
  return base::android::RegisterNativeMethods(
      env, kSyncRegisteredMethods, arraysize(kSyncRegisteredMethods));
}

}  // namespace syncer
