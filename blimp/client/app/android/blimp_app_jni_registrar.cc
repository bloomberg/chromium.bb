// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/app/android/blimp_app_jni_registrar.h"

#include "base/android/jni_registrar.h"
#include "blimp/client/app/android/blimp_contents_display.h"
#include "blimp/client/app/android/blimp_environment.h"
#include "blimp/client/app/android/blimp_library_loader.h"
#include "components/safe_json/android/component_jni_registrar.h"

namespace blimp {
namespace client {
namespace {

base::android::RegistrationMethod kBlimpRegistrationMethods[] = {
    {"BlimpLibraryLoader", RegisterBlimpLibraryLoaderJni},
    {"BlimpEnvironment", BlimpEnvironment::RegisterJni},
    {"BlimpContentsDisplay", app::BlimpContentsDisplay::RegisterJni},
    {"SafeJson", safe_json::android::RegisterSafeJsonJni},
};

}  // namespace

bool RegisterBlimpAppJni(JNIEnv* env) {
  return base::android::RegisterNativeMethods(
      env, kBlimpRegistrationMethods, arraysize(kBlimpRegistrationMethods));
}

}  // namespace client
}  // namespace blimp
