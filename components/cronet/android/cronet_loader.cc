// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/base_jni_registrar.h"
#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "base/at_exit.h"
#include "components/cronet/android/chromium_url_request.h"
#include "components/cronet/android/chromium_url_request_context.h"
#include "net/android/net_jni_registrar.h"
#include "url/android/url_jni_registrar.h"
#include "url/url_util.h"

#if !defined(USE_ICU_ALTERNATIVES_ON_ANDROID)
#include "base/i18n/icu_util.h"
#endif

namespace cronet {
namespace {

const base::android::RegistrationMethod kCronetRegisteredMethods[] = {
    {"BaseAndroid", base::android::RegisterJni},
    {"ChromiumUrlRequest", cronet::ChromiumUrlRequestRegisterJni},
    {"ChromiumUrlRequestContext", cronet::ChromiumUrlRequestContextRegisterJni},
    {"NetAndroid", net::android::RegisterJni},
    {"UrlAndroid", url::android::RegisterJni},
};

base::AtExitManager* g_at_exit_manager = NULL;

}  // namespace

// Checks the available version of JNI. Also, caches Java reflection artifacts.
jint CronetOnLoad(JavaVM* vm, void* reserved) {
  JNIEnv* env;
  if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
    return -1;
  }

  base::android::InitVM(vm);

  if (!base::android::RegisterNativeMethods(
          env, kCronetRegisteredMethods, arraysize(kCronetRegisteredMethods))) {
    return -1;
  }

  g_at_exit_manager = new base::AtExitManager();

#if !defined(USE_ICU_ALTERNATIVES_ON_ANDROID)
  base::i18n::InitializeICU();
#endif

  url::Initialize();

  return JNI_VERSION_1_6;
}

void CronetOnUnLoad(JavaVM* jvm, void* reserved) {
  if (g_at_exit_manager) {
    delete g_at_exit_manager;
    g_at_exit_manager = NULL;
  }
}

}  // namespace cronet
