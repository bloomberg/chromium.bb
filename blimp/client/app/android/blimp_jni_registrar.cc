// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/app/android/blimp_jni_registrar.h"

#include "base/android/jni_registrar.h"
#include "blimp/client/app/android/blimp_client_session_android.h"
#include "blimp/client/app/android/blimp_library_loader.h"
#include "blimp/client/app/android/blimp_view.h"
#include "blimp/client/app/android/tab_control_feature_android.h"
#include "blimp/client/app/android/toolbar.h"
#include "blimp/client/app/android/web_input_box.h"
#include "blimp/client/core/android/blimp_contents_factory.h"
#include "blimp/client/core/android/blimp_contents_impl_android.h"
#include "blimp/client/core/android/blimp_contents_observer_proxy.h"
#include "blimp/client/core/android/blimp_navigation_controller_impl_android.h"
#include "components/safe_json/android/component_jni_registrar.h"

namespace blimp {
namespace client {
namespace {

base::android::RegistrationMethod kBlimpRegistrationMethods[] = {
    {"BlimpClientSessionAndroid", BlimpClientSessionAndroid::RegisterJni},
    {"BlimpContentsFactory", RegisterBlimpContentsFactoryJni},
    {"BlimpContentsImpl", BlimpContentsImplAndroid::RegisterJni},
    {"BlimpContentsObserverProxy", BlimpContentsObserverProxy::RegisterJni},
    {"BlimpLibraryLoader", RegisterBlimpLibraryLoaderJni},
    {"BlimpNavigationControllerImplAndroid",
     BlimpNavigationControllerImplAndroid::RegisterJni},
    {"BlimpView", BlimpView::RegisterJni},
    {"SafeJson", safe_json::android::RegisterSafeJsonJni},
    {"TabControlFeatureAndroid", TabControlFeatureAndroid::RegisterJni},
    {"Toolbar", Toolbar::RegisterJni},
    {"WebInputBox", WebInputBox::RegisterJni},
};

}  // namespace

bool RegisterBlimpJni(JNIEnv* env) {
  return base::android::RegisterNativeMethods(
      env, kBlimpRegistrationMethods, arraysize(kBlimpRegistrationMethods));
}

}  // namespace client
}  // namespace blimp
