// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/engagement/site_engagement_service_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/profiles/profile_android.h"
#include "jni/SiteEngagementService_jni.h"
#include "url/gurl.h"

using base::android::JavaParamRef;

// static
bool SiteEngagementServiceAndroid::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

// static
const base::android::ScopedJavaGlobalRef<jobject>&
SiteEngagementServiceAndroid::GetOrCreate(JNIEnv* env,
                                          SiteEngagementService* service) {
  SiteEngagementServiceAndroid* android_service = service->GetAndroidService();
  if (!android_service) {
    service->SetAndroidService(
        base::MakeUnique<SiteEngagementServiceAndroid>(env, service));
    android_service = service->GetAndroidService();
  }

  return android_service->java_service_;
}

SiteEngagementServiceAndroid::SiteEngagementServiceAndroid(
    JNIEnv* env,
    SiteEngagementService* service)
    : service_(service) {
  java_service_.Reset(Java_SiteEngagementService_create(
      env, reinterpret_cast<uintptr_t>(this)));
}

SiteEngagementServiceAndroid::~SiteEngagementServiceAndroid() {
  Java_SiteEngagementService_onNativeDestroyed(
      base::android::AttachCurrentThread(), java_service_);
  java_service_.Reset();
}

double SiteEngagementServiceAndroid::GetScore(
    JNIEnv* env,
    const JavaParamRef<jobject>& caller,
    const JavaParamRef<jstring>& jurl) const {
  if (!jurl)
    return 0;

  return service_->GetScore(
      GURL(base::android::ConvertJavaStringToUTF16(env, jurl)));
}

void SiteEngagementServiceAndroid::ResetScoreForURL(
    JNIEnv* env,
    const JavaParamRef<jobject>& caller,
    const JavaParamRef<jstring>& jurl,
    double score) {
  if (jurl) {
    service_->ResetScoreForURL(
        GURL(base::android::ConvertJavaStringToUTF16(env, jurl)), score);
  }
}

base::android::ScopedJavaLocalRef<jobject> SiteEngagementServiceForProfile(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz,
    const JavaParamRef<jobject>& jprofile) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(jprofile);
  SiteEngagementService* service = SiteEngagementService::Get(profile);
  DCHECK(service);

  return base::android::ScopedJavaLocalRef<jobject>(
      SiteEngagementServiceAndroid::GetOrCreate(env, service));
}
