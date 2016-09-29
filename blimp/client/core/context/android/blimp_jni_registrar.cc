// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/public/android/blimp_jni_registrar.h"

#include "base/android/jni_registrar.h"
#include "blimp/client/core/contents/android/blimp_contents_jni_registrar.h"
#include "blimp/client/core/context/android/blimp_client_context_impl_android.h"
#include "blimp/client/core/settings/android/blimp_settings_android.h"

namespace blimp {
namespace client {
namespace {

base::android::RegistrationMethod kBlimpRegistrationMethods[] = {
    {"BlimpClientContextImplAndroid",
     BlimpClientContextImplAndroid::RegisterJni},
    {"BlimpContentsJni", RegisterBlimpContentsJni},
    {"BlimpSettingsAndroid", BlimpSettingsAndroid::RegisterJni},
};

}  // namespace

// This method is declared in
// //blimp/client/public/android/blimp_jni_registrar.h, and either this function
// or the one in //blimp/client/core/android/dummy_blimp_jni_registrar.cc
// should be linked in to any binary blimp::client::RegisterBlimpJni.
bool RegisterBlimpJni(JNIEnv* env) {
  return base::android::RegisterNativeMethods(
      env, kBlimpRegistrationMethods, arraysize(kBlimpRegistrationMethods));
}

}  // namespace client
}  // namespace blimp
