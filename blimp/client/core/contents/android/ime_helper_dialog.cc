// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/core/contents/android/ime_helper_dialog.h"

#include "base/android/jni_string.h"
#include "base/callback_helpers.h"
#include "jni/ImeHelperDialog_jni.h"
#include "ui/android/window_android.h"

namespace blimp {
namespace client {

// static
bool ImeHelperDialog::RegisterJni(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

ImeHelperDialog::ImeHelperDialog(ui::WindowAndroid* window) {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_obj_.Reset(Java_ImeHelperDialog_create(
      env, reinterpret_cast<intptr_t>(this), window->GetJavaObject()));
}

ImeHelperDialog::~ImeHelperDialog() {
  Java_ImeHelperDialog_clearNativePtr(base::android::AttachCurrentThread(),
                                      java_obj_);
}

void ImeHelperDialog::OnShowImeRequested(
    ui::TextInputType input_type,
    const std::string& text,
    const ImeFeature::ShowImeCallback& callback) {
  text_submit_callback_ = callback;

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

void ImeHelperDialog::OnImeTextEntered(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jobj,
    const base::android::JavaParamRef<jstring>& text) {
  std::string text_input = base::android::ConvertJavaStringToUTF8(env, text);
  base::ResetAndReturn(&text_submit_callback_).Run(text_input);
}

}  // namespace client
}  // namespace blimp
