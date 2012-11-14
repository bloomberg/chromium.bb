// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/components/web_contents_delegate_android/component_jni_registrar.h"

#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "content/components/web_contents_delegate_android/web_contents_delegate_android.h"

namespace content {

static base::android::RegistrationMethod kComponentRegisteredMethods[] = {
  { "WebContentsDelegateAndroid", RegisterWebContentsDelegateAndroid },
};

bool RegisterWebContentsDelegateAndroidJni(JNIEnv* env) {
  return RegisterNativeMethods(env,
      kComponentRegisteredMethods, arraysize(kComponentRegisteredMethods));
}

} // namespace content

