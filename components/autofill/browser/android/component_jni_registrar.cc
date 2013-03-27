// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/browser/android/component_jni_registrar.h"

#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "components/autofill/browser/android/auxiliary_profile_loader_android.h"

namespace components {

static base::android::RegistrationMethod kComponentRegisteredMethods[] = {
  { "RegisterAuxiliaryProfileLoader",
      autofill::RegisterAuxiliaryProfileLoader },
};

bool RegisterAutofillAndroidJni(JNIEnv* env) {
  return RegisterNativeMethods(env,
      kComponentRegisteredMethods, arraysize(kComponentRegisteredMethods));
}

} // namespace components
