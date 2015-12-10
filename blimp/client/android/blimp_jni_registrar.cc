// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/android/blimp_jni_registrar.h"

#include "base/android/jni_registrar.h"
#include "blimp/client/android/blimp_library_loader.h"
#include "blimp/client/android/blimp_view.h"
#include "blimp/client/android/toolbar.h"
#include "blimp/client/session/blimp_client_session_android.h"
#include "blimp/client/session/tab_control_feature_android.h"

namespace {

base::android::RegistrationMethod kBlimpRegistrationMethods[] = {
    {"BlimpLibraryLoader", blimp::RegisterBlimpLibraryLoaderJni},
    {"Toolbar", blimp::Toolbar::RegisterJni},
    {"BlimpView", blimp::BlimpView::RegisterJni},
    {"BlimpClientSessionAndroid",
     blimp::BlimpClientSessionAndroid::RegisterJni},
    {"TabControlFeatureAndroid", blimp::TabControlFeatureAndroid::RegisterJni},
};

}  // namespace

namespace blimp {

bool RegisterBlimpJni(JNIEnv* env) {
  return base::android::RegisterNativeMethods(
      env, kBlimpRegistrationMethods, arraysize(kBlimpRegistrationMethods));
}

}  // namespace blimp
