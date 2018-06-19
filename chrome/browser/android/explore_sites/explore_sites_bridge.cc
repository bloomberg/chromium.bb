// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/explore_sites/explore_sites_bridge.h"

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "jni/ExploreSitesBridge_jni.h"
#include "jni/ExploreSitesCategoryTile_jni.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace explore_sites {

using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;
using base::android::ScopedJavaGlobalRef;

namespace {

void GotNTPCategoriesFromJson(
    const ScopedJavaGlobalRef<jobject>& j_callback_ref,
    const ScopedJavaGlobalRef<jobject>& j_result_ref,
    std::unique_ptr<NTPJsonFetcher> fetcher,
    std::unique_ptr<NTPCatalog> catalog) {
  JNIEnv* env = base::android::AttachCurrentThread();
  if (catalog) {
    for (NTPCatalog::Category category : catalog->categories) {
      Java_ExploreSitesCategoryTile_createInList(
          env, j_result_ref,
          base::android::ConvertUTF8ToJavaString(env, category.id),
          base::android::ConvertUTF8ToJavaString(env, category.icon_url.spec()),
          base::android::ConvertUTF8ToJavaString(env, category.title));
    }
  }

  base::android::RunCallbackAndroid(j_callback_ref, j_result_ref);
}

}  // namespace

// static
void JNI_ExploreSitesBridge_GetNtpCategories(
    JNIEnv* env,
    const JavaParamRef<jclass>& j_caller,
    const JavaParamRef<jobject>& j_profile,
    const JavaParamRef<jobject>& j_result_obj,
    const JavaParamRef<jobject>& j_callback_obj) {
  NTPJsonFetcher* ntp_fetcher =
      new NTPJsonFetcher(ProfileAndroid::FromProfileAndroid(j_profile));

  ntp_fetcher->Start(base::BindOnce(
      &GotNTPCategoriesFromJson, ScopedJavaGlobalRef<jobject>(j_callback_obj),
      ScopedJavaGlobalRef<jobject>(j_result_obj),
      base::WrapUnique(ntp_fetcher)));
}

}  // namespace explore_sites
