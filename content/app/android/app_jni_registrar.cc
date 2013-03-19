// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/app/android/app_jni_registrar.h"

#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "content/app/android/child_process_service.h"
#include "content/app/android/content_main.h"

namespace {

base::android::RegistrationMethod kContentRegisteredMethods[] = {
  { "ContentMain", content::RegisterContentMain },
  { "ChildProcessService", content::RegisterChildProcessService },
};

}  // namespace

namespace content {
namespace android {

bool RegisterAppJni(JNIEnv* env) {
  return RegisterNativeMethods(env, kContentRegisteredMethods,
                               arraysize(kContentRegisteredMethods));
}

}  // namespace android
}  // namespace content
