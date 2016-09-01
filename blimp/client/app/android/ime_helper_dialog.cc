// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/app/android/ime_helper_dialog.h"

#include "base/android/jni_string.h"
#include "blimp/client/app/android/blimp_client_session_android.h"
#include "blimp/client/core/contents/ime_feature.h"
#include "jni/ImeHelperDialog_jni.h"
#include "ui/base/ime/text_input_type.h"

using base::android::JavaParamRef;

namespace blimp {
namespace client {

static jlong Init(JNIEnv* env,
                  const JavaParamRef<jobject>& jobj,
                  const JavaParamRef<jobject>& blimp_client_session) {
  BlimpClientSession* client_session =
      BlimpClientSessionAndroid::FromJavaObject(env,
                                                blimp_client_session.obj());
  return reinterpret_cast<intptr_t>(
      new ImeHelperDialog(env, jobj, client_session->GetImeFeature()));
}

// static
bool ImeHelperDialog::RegisterJni(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

ImeHelperDialog::ImeHelperDialog(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jobj,
    ImeFeature* ime_feature) {
  java_obj_.Reset(env, jobj);
  ime_feature_ = ime_feature;
  ime_feature_->set_delegate(this);
}

ImeHelperDialog::~ImeHelperDialog() {
  ime_feature_->set_delegate(nullptr);
}

void ImeHelperDialog::Destroy(JNIEnv* env, const JavaParamRef<jobject>& jobj) {
  delete this;
}

void ImeHelperDialog::OnShowImeRequested(ui::TextInputType input_type,
                                         const std::string& text) {
  JNIEnv* env = base::android::AttachCurrentThread();
  DCHECK_NE(ui::TEXT_INPUT_TYPE_NONE, input_type);
  Java_ImeHelperDialog_onShowImeRequested(
      env, java_obj_, input_type,
      base::android::ConvertUTF8ToJavaString(env, text));
}

void ImeHelperDialog::OnHideImeRequested() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ImeHelperDialog_onHideImeRequested(env, java_obj_);
}

void ImeHelperDialog::OnImeTextEntered(JNIEnv* env,
                                       const JavaParamRef<jobject>& jobj,
                                       const JavaParamRef<jstring>& text) {
  std::string textInput = base::android::ConvertJavaStringToUTF8(env, text);
  ime_feature_->OnImeTextEntered(textInput);
}

}  // namespace client
}  // namespace blimp
