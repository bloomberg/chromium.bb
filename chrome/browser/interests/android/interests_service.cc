// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/interests/android/interests_service.h"

#include <stddef.h>
#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/browser/interests/interests_fetcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "jni/InterestsService_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

namespace {

ScopedJavaLocalRef<jobjectArray> ConvertInterestsToJava(
    JNIEnv* env,
    std::unique_ptr<std::vector<InterestsFetcher::Interest>> interests) {
  if (!interests)
    return ScopedJavaLocalRef<jobjectArray>();

  ScopedJavaLocalRef<jobjectArray> j_interests =
      Java_InterestsService_createInterestsArray(env, interests->size());

  for (size_t i = 0; i != interests->size(); i++) {
    const InterestsFetcher::Interest& interest = (*interests)[i];
    ScopedJavaLocalRef<jobject> j_interest =
        Java_InterestsService_createInterest(
            env,
            ConvertUTF8ToJavaString(env, interest.name).obj(),
            ConvertUTF8ToJavaString(env, interest.image_url.spec()).obj(),
            interest.relevance);

    env->SetObjectArrayElement(j_interests.obj(), i, j_interest.obj());
  }

  return j_interests;
}

}  // namespace

InterestsService::InterestsService(Profile* profile)
    : profile_(profile), weak_ptr_factory_(this) {}

InterestsService::~InterestsService() {}

void InterestsService::Destroy(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  delete this;
}

void InterestsService::GetInterests(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& j_callback_obj) {
  ScopedJavaGlobalRef<jobject> j_callback(env, j_callback_obj);

  std::unique_ptr<InterestsFetcher> fetcher =
      InterestsFetcher::CreateFromProfile(profile_);
  InterestsFetcher* fetcher_raw_ptr = fetcher.get();

  InterestsFetcher::InterestsCallback callback = base::Bind(
      &InterestsService::OnObtainedInterests, weak_ptr_factory_.GetWeakPtr(),
      base::Passed(std::move(fetcher)), j_callback);

  fetcher_raw_ptr->FetchInterests(callback);
}

// static
bool InterestsService::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

void InterestsService::OnObtainedInterests(
    std::unique_ptr<InterestsFetcher> fetcher,
    const ScopedJavaGlobalRef<jobject>& j_callback,
    std::unique_ptr<std::vector<InterestsFetcher::Interest>> interests) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobjectArray> j_interests =
      ConvertInterestsToJava(env, std::move(interests));
  Java_GetInterestsCallback_onInterestsAvailable(env,
                                                 j_callback.obj(),
                                                 j_interests.obj());
}

static jlong Init(JNIEnv* env,
                  const JavaParamRef<jobject>& jobj,
                  const JavaParamRef<jobject>& jprofile) {
  InterestsService* interests_service =
      new InterestsService(ProfileAndroid::FromProfileAndroid(jprofile));
  return reinterpret_cast<intptr_t>(interests_service);
}
