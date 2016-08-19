// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/core/contents/android/blimp_contents_jni_registrar.h"

#include "base/android/jni_registrar.h"
#include "blimp/client/core/contents/android/blimp_contents_impl_android.h"
#include "blimp/client/core/contents/android/blimp_contents_observer_proxy.h"
#include "blimp/client/core/contents/android/blimp_navigation_controller_impl_android.h"

namespace blimp {
namespace client {
namespace {

base::android::RegistrationMethod kBlimpRegistrationMethods[] = {
    {"BlimpContentsImplAndroid", BlimpContentsImplAndroid::RegisterJni},
    {"BlimpContentsObserverProxy", BlimpContentsObserverProxy::RegisterJni},
    {"BlimpNavigationControllerImplAndroid",
     BlimpNavigationControllerImplAndroid::RegisterJni},
};

}  // namespace

bool RegisterBlimpContentsJni(JNIEnv* env) {
  return base::android::RegisterNativeMethods(
      env, kBlimpRegistrationMethods, arraysize(kBlimpRegistrationMethods));
}

}  // namespace client
}  // namespace blimp
