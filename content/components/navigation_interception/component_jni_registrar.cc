// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/components/navigation_interception/component_jni_registrar.h"

#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "content/components/navigation_interception/intercept_navigation_delegate.h"

namespace content {

static base::android::RegistrationMethod kComponentRegisteredMethods[] = {
  { "InterceptNavigationDelegate", RegisterInterceptNavigationDelegate },
};

bool RegisterNavigationInterceptionJni(JNIEnv* env) {
  return RegisterNativeMethods(env,
      kComponentRegisteredMethods, arraysize(kComponentRegisteredMethods));
}

}  // namespace content
