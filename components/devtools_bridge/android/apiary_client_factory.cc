// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/devtools_bridge/android/apiary_client_factory.h"

#include "base/android/jni_string.h"
#include "google_apis/google_api_keys.h"
#include "jni/ApiaryClientFactory_jni.h"

using base::android::ConvertUTF8ToJavaString;

namespace devtools_bridge {
namespace android {

// static
bool ApiaryClientFactory::RegisterNatives(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

// static
jstring GetAPIKey(JNIEnv* env, jobject jcaller) {
  return ConvertUTF8ToJavaString(env, google_apis::GetAPIKey()).Release();
}

// static
jstring GetOAuthClientId(JNIEnv* env, jobject jcaller) {
  return ConvertUTF8ToJavaString(
             env, google_apis::GetOAuth2ClientID(
                      google_apis::OAuth2Client::CLIENT_MAIN)).Release();
}

// static
jstring GetOAuthClientSecret(JNIEnv* env, jobject jcaller) {
  return ConvertUTF8ToJavaString(
             env, google_apis::GetOAuth2ClientSecret(
                      google_apis::OAuth2Client::CLIENT_MAIN)).Release();
}

}  // namespace android
}  // namespace devtools_bridge
