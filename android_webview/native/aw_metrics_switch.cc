// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/native/aw_metrics_switch.h"

#include "android_webview/browser/aw_metrics_service_client.h"
#include "android_webview/jni/AwMetricsServiceClient_jni.h"

namespace android_webview {

static void SetMetricsEnabled(
    JNIEnv* env,
    const base::android::JavaParamRef<jclass>& jcaller,
    jboolean enabled) {
  AwMetricsServiceClient::GetInstance()->SetMetricsEnabled(enabled);
}

bool RegisterAwMetricsServiceClient(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace android_webview
