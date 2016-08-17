// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/browsing_data/browsing_data_counter_bridge.h"

#include "base/android/jni_string.h"
#include "chrome/browser/browsing_data/browsing_data_counter_factory.h"
#include "chrome/browser/browsing_data/browsing_data_counter_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "components/browsing_data/core/browsing_data_utils.h"
#include "jni/BrowsingDataCounterBridge_jni.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

BrowsingDataCounterBridge::BrowsingDataCounterBridge(
    JNIEnv* env, const JavaParamRef<jobject>& obj, jint data_type)
    : jobject_(obj) {
  DCHECK_GE(data_type, 0);
  DCHECK_LT(data_type, browsing_data::NUM_TYPES);

  std::string pref;
  if (!browsing_data::GetDeletionPreferenceFromDataType(
          static_cast<browsing_data::BrowsingDataType>(data_type), &pref)) {
    return;
  }

  Profile* profile =
      ProfileManager::GetActiveUserProfile()->GetOriginalProfile();
  counter_ = BrowsingDataCounterFactory::GetForProfileAndPref(profile, pref);

  if (!counter_)
    return;

  counter_->Init(profile->GetPrefs(),
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
    std::unique_ptr<browsing_data::BrowsingDataCounter::Result> result) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> result_string =
      base::android::ConvertUTF16ToJavaString(
          env, GetChromeCounterTextFromResult(result.get()));
  Java_BrowsingDataCounterBridge_onBrowsingDataCounterFinished(env, jobject_,
                                                               result_string);
}

static jlong Init(
    JNIEnv* env, const JavaParamRef<jobject>& obj, int data_type) {
  return reinterpret_cast<intptr_t>(
      new BrowsingDataCounterBridge(env, obj, data_type));
}
