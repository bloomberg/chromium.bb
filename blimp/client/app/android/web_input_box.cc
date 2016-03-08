// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_string.h"
#include "blimp/client/app/android/blimp_client_session_android.h"
#include "blimp/client/app/android/web_input_box.h"
#include "jni/WebInputBox_jni.h"

namespace blimp {
namespace client {

static jlong Init(JNIEnv* env,
                  const JavaParamRef<jobject>& jobj,
                  const JavaParamRef<jobject>& blimp_client_session) {
  return reinterpret_cast<intptr_t>(new WebInputBox(env, jobj));
}

// static
bool WebInputBox::RegisterJni(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

WebInputBox::WebInputBox(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& jobj) {
  java_obj_.Reset(env, jobj);
}

WebInputBox::~WebInputBox() {}

void WebInputBox::Destroy(JNIEnv* env, const JavaParamRef<jobject>& jobj) {
  delete this;
}

void WebInputBox::OnImeRequested(bool show) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_WebInputBox_onImeRequested(env, java_obj_.obj(), show);
}

void WebInputBox::OnImeTextEntered(JNIEnv* env,
                                   const JavaParamRef<jobject>& jobj,
                                   const JavaParamRef<jstring>& text) {
  // TODO(shaktisahu): Send text to browser.
}

}  // namespace client
}  // namespace blimp
