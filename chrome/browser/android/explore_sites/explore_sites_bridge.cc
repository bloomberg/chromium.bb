// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/explore_sites/explore_sites_bridge.h"

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "jni/ExploreSitesBridge_jni.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace explore_sites {

static jlong JNI_ExploreSitesBridge_Init(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jobject>& j_profile) {
  ExploreSitesBridge* explore_sites_bridge = new ExploreSitesBridge(j_profile);
  return reinterpret_cast<intptr_t>(explore_sites_bridge);
}

ExploreSitesBridge::ExploreSitesBridge(
    const base::android::JavaRef<jobject>& j_profile)
    : weak_ptr_factory_(this) {}

void ExploreSitesBridge::Destroy(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  delete this;
}

void ExploreSitesBridge::GetNtpCategories(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jstring>& url,
    const base::android::JavaParamRef<jobject>& j_result_obj,
    const base::android::JavaParamRef<jobject>& j_callback_obj) {}

ExploreSitesBridge::~ExploreSitesBridge() {}

}  // namespace explore_sites
