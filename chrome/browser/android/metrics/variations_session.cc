// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/metrics/variations_session.h"

#include "base/android/jni_string.h"
#include "chrome/browser/browser_process.h"
#include "components/variations/service/variations_service.h"
#include "jni/VariationsSession_jni.h"

using base::android::JavaParamRef;

namespace {

// Tracks whether VariationsService::OnAppEnterForeground() has been called
// previously, in order to set the restrict mode param before the first call.
bool g_on_app_enter_foreground_called = false;

}  // namespace

static void StartVariationsSession(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& jrestrict_mode) {
  DCHECK(g_browser_process);

  variations::VariationsService* variations_service =
      g_browser_process->variations_service();
  // Triggers an OnAppEnterForeground on the VariationsService. This may fetch
  // a new seed.
  if (variations_service) {
    std::string restrict_mode =
        base::android::ConvertJavaStringToUTF8(env, jrestrict_mode);
    if (!restrict_mode.empty() && !g_on_app_enter_foreground_called)
      variations_service->SetRestrictMode(restrict_mode);
    variations_service->OnAppEnterForeground();
    g_on_app_enter_foreground_called = true;
  }
}

namespace chrome {
namespace android {

// Register native methods
bool RegisterVariationsSession(JNIEnv* env) { return RegisterNativesImpl(env); }

}  // namespace android
}  // namespace chrome
