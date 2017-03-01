// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/resource_prefetch_predictor_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "chrome/browser/predictors/resource_prefetch_predictor.h"
#include "chrome/browser/predictors/resource_prefetch_predictor_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "jni/ResourcePrefetchPredictor_jni.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace predictors {

namespace {
ResourcePrefetchPredictor* ResourcePrefetchPredictorFromProfileAndroid(
    const JavaParamRef<jobject>& j_profile) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(j_profile);
  if (!profile)
    return nullptr;
  return ResourcePrefetchPredictorFactory::GetInstance()->GetForProfile(
      profile);
}

}  // namespace

static jboolean StartInitialization(JNIEnv* env,
                                    const JavaParamRef<jclass>& clazz,
                                    const JavaParamRef<jobject>& j_profile) {
  auto* predictor = ResourcePrefetchPredictorFromProfileAndroid(j_profile);
  if (!predictor)
    return false;
  predictor->StartInitialization();
  return true;
}

static jboolean StartPrefetching(JNIEnv* env,
                                 const JavaParamRef<jclass>& clazz,
                                 const JavaParamRef<jobject>& j_profile,
                                 const JavaParamRef<jstring>& j_url) {
  auto* predictor = ResourcePrefetchPredictorFromProfileAndroid(j_profile);
  if (!predictor)
    return false;
  GURL url = GURL(base::android::ConvertJavaStringToUTF16(env, j_url));
  predictor->StartPrefetching(url, PrefetchOrigin::EXTERNAL);

  return true;
}

static jboolean StopPrefetching(JNIEnv* env,
                                const JavaParamRef<jclass>& clazz,
                                const JavaParamRef<jobject>& j_profile,
                                const JavaParamRef<jstring>& j_url) {
  auto* predictor = ResourcePrefetchPredictorFromProfileAndroid(j_profile);
  if (!predictor)
    return false;
  GURL url = GURL(base::android::ConvertJavaStringToUTF16(env, j_url));
  predictor->StopPrefetching(url);

  return true;
}

bool RegisterResourcePrefetchPredictor(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace predictors
