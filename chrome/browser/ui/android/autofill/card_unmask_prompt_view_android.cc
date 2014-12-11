// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/card_unmask_prompt_view_android.h"

#include "content/public/browser/web_contents.h"
#include "jni/CardUnmaskBridge_jni.h"
#include "ui/base/android/view_android.h"
#include "ui/base/android/window_android.h"

namespace autofill {

CardUnmaskPromptView* CardUnmaskPromptView::CreateAndShow(
    content::WebContents* web_contents,
    const UnmaskCallback& finished) {
  CardUnmaskPromptViewAndroid* view =
      new CardUnmaskPromptViewAndroid(web_contents, finished);
  view->Show();
  return view;
}

CardUnmaskPromptViewAndroid::CardUnmaskPromptViewAndroid(
    content::WebContents* web_contents,
    const UnmaskCallback& finished)
    : web_contents_(web_contents), finished_(finished) {
}

CardUnmaskPromptViewAndroid::~CardUnmaskPromptViewAndroid() {
  finished_.Run(response_);
}

void CardUnmaskPromptViewAndroid::Show() {
  JNIEnv* env = base::android::AttachCurrentThread();
  ui::ViewAndroid* view_android = web_contents_->GetNativeView();

  java_object_.Reset(Java_CardUnmaskBridge_create(
      env, reinterpret_cast<intptr_t>(this),
      view_android->GetWindowAndroid()->GetJavaObject().obj()));

  Java_CardUnmaskBridge_show(env, java_object_.obj());
}

void CardUnmaskPromptViewAndroid::PromptDismissed(JNIEnv* env,
                                                  jobject obj,
                                                  jstring response) {
  response_ = base::android::ConvertJavaStringToUTF16(env, response);
  delete this;
}

// static
bool CardUnmaskPromptViewAndroid::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace autofill
