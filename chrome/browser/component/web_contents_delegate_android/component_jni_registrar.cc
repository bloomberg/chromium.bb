// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component/web_contents_delegate_android/component_jni_registrar.h"

#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "chrome/browser/component/web_contents_delegate_android/web_contents_delegate_android.h"

namespace web_contents_delegate_android {

static base::android::RegistrationMethod kComponentRegisteredMethods[] = {
  { "WebContentsDelegateAndroid", RegisterWebContentsDelegateAndroid },
};

bool RegisterJni(JNIEnv* env) {
  return RegisterNativeMethods(env,
      kComponentRegisteredMethods, arraysize(kComponentRegisteredMethods));
}

} // namespace web_contents_delegate_android

