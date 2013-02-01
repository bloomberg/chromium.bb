// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/native/js_result_handler.h"

#include "base/android/jni_string.h"
#include "content/public/browser/android/content_view_core.h"
#include "content/public/browser/javascript_dialog_manager.h"
#include "jni/JsResultHandler_jni.h"

using base::android::ConvertJavaStringToUTF16;
using content::ContentViewCore;

namespace android_webview {

void ConfirmJsResult(JNIEnv* env,
                     jobject obj,
                     jint dialogPointer,
                     jstring promptResult) {
  content::JavaScriptDialogManager::DialogClosedCallback* dialog_ =
      reinterpret_cast<content::JavaScriptDialogManager::DialogClosedCallback*>(
          dialogPointer);
  string16 prompt_text;
  if (promptResult) {
    prompt_text = ConvertJavaStringToUTF16(env, promptResult);
  }
  dialog_->Run(true,prompt_text);
}

void CancelJsResult(JNIEnv* env, jobject obj, jint dialogPointer) {
  content::JavaScriptDialogManager::DialogClosedCallback* dialog_ =
      reinterpret_cast<content::JavaScriptDialogManager::DialogClosedCallback*>(
          dialogPointer);
  dialog_->Run(false, string16());
}

base::android::ScopedJavaLocalRef<jobject> createJsResultHandler(
    JNIEnv* env,
    const content::JavaScriptDialogManager::DialogClosedCallback* dialog) {
  return Java_JsResultHandler_create(env, reinterpret_cast<jint>(dialog));
}

bool RegisterJsResultHandler(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace android_webview
