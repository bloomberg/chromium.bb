// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/offline_pages/offline_page_bridge.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "components/offline_pages/offline_page_model.h"
#include "content/public/browser/browser_context.h"
#include "jni/OfflinePageBridge_jni.h"

namespace offline_pages {
namespace android {

OfflinePageBridge::OfflinePageBridge(JNIEnv* env,
                                     jobject obj,
                                     content::BrowserContext* browser_context)
    : weak_java_ref_(env, obj),
      offline_page_model_(nullptr),
      browser_context_(browser_context) {
  // TODO(fgorski): Pull offline pages model from the factory once it is
  // available.
}

void OfflinePageBridge::Destroy(JNIEnv*, jobject) {
  delete this;
}

static jlong Init(JNIEnv* env, jobject obj, jobject j_profile) {
  return reinterpret_cast<jlong>(new OfflinePageBridge(
      env, obj, ProfileAndroid::FromProfileAndroid(j_profile)));
}

bool RegisterOfflinePageBridge(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace android
}  // namespace offline_pages
