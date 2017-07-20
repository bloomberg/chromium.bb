// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"

#include "chrome/browser/predictors/loading_predictor.h"
#include "chrome/browser/predictors/loading_predictor_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "jni/LoadingPredictor_jni.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace predictors {

namespace {
LoadingPredictor* LoadingPredictorFromProfileAndroid(
    const JavaParamRef<jobject>& j_profile) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(j_profile);
  if (!profile)
    return nullptr;
  return LoadingPredictorFactory::GetInstance()->GetForProfile(profile);
}

}  // namespace

static jboolean StartInitialization(JNIEnv* env,
                                    const JavaParamRef<jclass>& clazz,
                                    const JavaParamRef<jobject>& j_profile) {
  auto* loading_predictor = LoadingPredictorFromProfileAndroid(j_profile);
  if (!loading_predictor)
    return false;
  loading_predictor->StartInitialization();
  return true;
}

static jboolean PrepareForPageLoad(JNIEnv* env,
                                   const JavaParamRef<jclass>& clazz,
                                   const JavaParamRef<jobject>& j_profile,
                                   const JavaParamRef<jstring>& j_url) {
  auto* loading_predictor = LoadingPredictorFromProfileAndroid(j_profile);
  if (!loading_predictor)
    return false;
  GURL url = GURL(base::android::ConvertJavaStringToUTF16(env, j_url));
  loading_predictor->PrepareForPageLoad(url, HintOrigin::EXTERNAL);

  return true;
}

static jboolean CancelPageLoadHint(JNIEnv* env,
                                   const JavaParamRef<jclass>& clazz,
                                   const JavaParamRef<jobject>& j_profile,
                                   const JavaParamRef<jstring>& j_url) {
  auto* loading_predictor = LoadingPredictorFromProfileAndroid(j_profile);
  if (!loading_predictor)
    return false;
  GURL url = GURL(base::android::ConvertJavaStringToUTF16(env, j_url));
  loading_predictor->CancelPageLoadHint(url);

  return true;
}

}  // namespace predictors
