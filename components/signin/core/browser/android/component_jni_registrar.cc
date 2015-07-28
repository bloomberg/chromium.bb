// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/android/component_jni_registrar.h"

#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "base/basictypes.h"
#include "components/signin/core/browser/child_account_info_fetcher_android.h"

namespace signin {
namespace android {

static base::android::RegistrationMethod kSigninRegisteredMethods[] = {
    {"ChildAccountInfoFetcher", ChildAccountInfoFetcherAndroid::Register},
};

bool RegisterSigninJni(JNIEnv* env) {
  return base::android::RegisterNativeMethods(
      env, kSigninRegisteredMethods, arraysize(kSigninRegisteredMethods));
}

}  // namespace android
}  // namespace signin
