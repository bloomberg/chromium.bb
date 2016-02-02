// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/contextualsearch/contextual_search_tab_helper.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_change_registrar.h"
#include "jni/ContextualSearchTabHelper_jni.h"


ContextualSearchTabHelper::ContextualSearchTabHelper(JNIEnv* env,
                                                     jobject obj,
                                                     Profile* profile)
    : weak_java_ref_(env, obj),
      pref_change_registrar_(new PrefChangeRegistrar()),
      weak_factory_(this) {
  pref_change_registrar_->Init(profile->GetPrefs());
  pref_change_registrar_->Add(
      prefs::kContextualSearchEnabled,
      base::Bind(&ContextualSearchTabHelper::OnContextualSearchPrefChanged,
                 weak_factory_.GetWeakPtr()));
}

ContextualSearchTabHelper::~ContextualSearchTabHelper() {
  pref_change_registrar_->RemoveAll();
}

void ContextualSearchTabHelper::OnContextualSearchPrefChanged() {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> jobj = weak_java_ref_.get(env);
  Java_ContextualSearchTabHelper_onContextualSearchPrefChanged(env, jobj.obj());
}

void ContextualSearchTabHelper::Destroy(JNIEnv* env,
                                        const JavaParamRef<jobject>& obj) {
  delete this;
}

static jlong Init(JNIEnv* env,
                  const JavaParamRef<jobject>& obj,
                  const JavaParamRef<jobject>& java_profile) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(java_profile);
  CHECK(profile);
  ContextualSearchTabHelper* tab = new ContextualSearchTabHelper(
      env, obj, profile);
  return reinterpret_cast<intptr_t>(tab);
}

bool RegisterContextualSearchTabHelper(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
