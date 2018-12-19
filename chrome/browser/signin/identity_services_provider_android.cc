// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/signin/account_tracker_service_factory.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/oauth2_token_service_delegate_android.h"
#include "jni/IdentityServicesProvider_jni.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

static ScopedJavaLocalRef<jobject>
JNI_IdentityServicesProvider_GetAccountTrackerService(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_profile_android) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(j_profile_android);
  AccountTrackerService* service =
      AccountTrackerServiceFactory::GetForProfile(profile);
  return service->GetJavaObject();
}

static ScopedJavaLocalRef<jobject>
JNI_IdentityServicesProvider_GetOAuth2TokenService(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_profile_android) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(j_profile_android);
  ProfileOAuth2TokenService* service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile);
  OAuth2TokenServiceDelegateAndroid* delegate =
      static_cast<OAuth2TokenServiceDelegateAndroid*>(service->GetDelegate());
  return delegate->GetJavaObject();
}
