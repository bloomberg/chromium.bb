// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/android/test_support_jni_registrar.h"

#include "base/android/jni_registrar.h"
#include "base/test/android/multiprocess_test_client_service.h"
#include "base/test/multiprocess_test.h"

namespace base {
namespace android {

static RegistrationMethod kBaseTestSupportRegisteredMethods[] = {
    {"MultiprocessTestClientService", RegisterMultiprocessTestClientServiceJni},
};

bool RegisterTestSupportJni(JNIEnv* env) {
  return RegisterNativeMethods(env, kBaseTestSupportRegisteredMethods,
                               arraysize(kBaseTestSupportRegisteredMethods));
}

}  // namespace android
}  // namespace base
