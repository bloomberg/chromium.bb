// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/core/settings/android/blimp_settings_android.h"

#include "blimp/client/public/blimp_client_context.h"
#include "jni/AboutBlimpPreferences_jni.h"

namespace blimp {
namespace client {

// static
bool BlimpSettingsAndroid::RegisterJni(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

// static
BlimpSettingsAndroid* BlimpSettingsAndroid::FromJavaObject(JNIEnv* env,
                                                           jobject jobj) {
  return reinterpret_cast<BlimpSettingsAndroid*>(
      Java_AboutBlimpPreferences_getNativePtr(env, jobj));
}

static jlong Init(JNIEnv* env,
                  const base::android::JavaParamRef<jobject>& jobj) {
  return reinterpret_cast<intptr_t>(new BlimpSettingsAndroid(env, jobj));
}

BlimpSettingsAndroid::BlimpSettingsAndroid(JNIEnv* env, jobject jobj)
    : identity_source_(nullptr) {
  java_obj_.Reset(env, jobj);
}

BlimpSettingsAndroid::~BlimpSettingsAndroid() {
  Java_AboutBlimpPreferences_clearNativePtr(
      base::android::AttachCurrentThread(), java_obj_);
}

void BlimpSettingsAndroid::Destroy(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jobj) {
  DCHECK(identity_source_);
  identity_source_->RemoveObserver(this);
  delete this;
}

void BlimpSettingsAndroid::SetIdentitySource(IdentitySource* identity_source) {
  if (identity_source_) {
    identity_source_->RemoveObserver(this);
  }

  identity_source_ = identity_source;
  DCHECK(identity_source_);

  // Listen to sign in state change.
  identity_source->AddObserver(this);
}

void BlimpSettingsAndroid::OnSignedOut() {
  Java_AboutBlimpPreferences_onSignedOut(base::android::AttachCurrentThread(),
                                         java_obj_);
}

void BlimpSettingsAndroid::OnSignedIn() {
  Java_AboutBlimpPreferences_onSignedIn(base::android::AttachCurrentThread(),
                                        java_obj_);
}

void BlimpSettingsAndroid::OnActiveAccountLogout() {
  OnSignedOut();
}

void BlimpSettingsAndroid::OnActiveAccountLogin() {
  OnSignedIn();
}

}  // namespace client
}  // namespace blimp
