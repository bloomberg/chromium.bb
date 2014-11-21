// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/devtools_bridge/test/android/client/web_client_android.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "jni/WebClient_jni.h"

namespace devtools_bridge {
namespace android {

bool WebClientAndroid::RegisterNatives(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

WebClientAndroid::WebClientAndroid(Profile* profile)
    : impl_(WebClient::CreateInstance(profile, this)) {
}

WebClientAndroid::~WebClientAndroid() {
}

static jlong CreateWebClient(JNIEnv* env, jclass jcaller, jobject j_profile) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(j_profile);
  return reinterpret_cast<jlong>(new WebClientAndroid(profile));
}

static void DestroyWebClient(
    JNIEnv* env, jclass jcaller, jlong web_client_ptr) {
  delete reinterpret_cast<WebClientAndroid*>(web_client_ptr);
}

}  // namespace android
}  // namespace devtools_bridge
