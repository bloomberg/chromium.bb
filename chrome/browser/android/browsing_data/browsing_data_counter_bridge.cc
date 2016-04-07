// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/browsing_data/browsing_data_counter_bridge.h"

#include "base/android/jni_string.h"
#include "chrome/browser/browsing_data/browsing_data_counter_utils.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "jni/BrowsingDataCounterBridge_jni.h"

BrowsingDataCounterBridge::BrowsingDataCounterBridge(
    JNIEnv* env, const JavaParamRef<jobject>& obj, jint data_type)
    : jobject_(obj) {
  DCHECK_GE(data_type, 0);
  DCHECK_LT(data_type, BrowsingDataType::NUM_TYPES);

  std::string pref;
  if (!GetDeletionPreferenceFromDataType(
      static_cast<BrowsingDataType>(data_type), &pref)) {
    return;
  }

  counter_.reset(CreateCounterForPreference(pref));

  if (!counter_)
    return;

  counter_->Init(
      ProfileManager::GetActiveUserProfile()->GetOriginalProfile(),
      base::Bind(&BrowsingDataCounterBridge::onCounterFinished,
                 base::Unretained(this)));
  counter_->Restart();
}

BrowsingDataCounterBridge::~BrowsingDataCounterBridge() {
}

void BrowsingDataCounterBridge::Destroy(JNIEnv* env,
                                        const JavaParamRef<jobject>& obj) {
  delete this;
}

// static
bool BrowsingDataCounterBridge::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

void BrowsingDataCounterBridge::onCounterFinished(
    std::unique_ptr<BrowsingDataCounter::Result> result) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> result_string =
      base::android::ConvertUTF16ToJavaString(
          env, GetCounterTextFromResult(result.get()));
  Java_BrowsingDataCounterBridge_onBrowsingDataCounterFinished(
      env, jobject_.obj(), result_string.obj());
}

static jlong Init(
    JNIEnv* env, const JavaParamRef<jobject>& obj, int data_type) {
  return reinterpret_cast<intptr_t>(
      new BrowsingDataCounterBridge(env, obj, data_type));
}
