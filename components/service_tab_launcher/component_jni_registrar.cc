// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/service_tab_launcher/component_jni_registrar.h"

#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "components/service_tab_launcher/browser/android/service_tab_launcher.h"

namespace service_tab_launcher {

static base::android::RegistrationMethod kComponentRegisteredMethods[] = {
  { "ServiceTabLauncher", ServiceTabLauncher::RegisterServiceTabLauncher },
};

bool RegisterServiceTabLauncherJni(JNIEnv* env) {
  return RegisterNativeMethods(env,
      kComponentRegisteredMethods, arraysize(kComponentRegisteredMethods));
}

} // namespace service_tab_launcher

