// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/autofill_assistant/assistant_ui_controller_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "components/autofill_assistant/browser/assistant_controller.h"
#include "content/public/browser/web_contents.h"
#include "jni/AssistantUiController_jni.h"

using base::android::AttachCurrentThread;
using base::android::JavaParamRef;

namespace autofill_assistant {

AssistantUiControllerAndroid::AssistantUiControllerAndroid(
    JNIEnv* env,
    jobject jcaller,
    const JavaParamRef<jobject>& webContents)
    : ui_delegate_(nullptr) {
  java_assistant_ui_controller_.Reset(env, jcaller);

  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(webContents);
  DCHECK(web_contents);
  AssistantController::CreateAndStartForWebContents(
      web_contents, std::unique_ptr<AssistantUiController>(this));
  DCHECK(ui_delegate_);
}

AssistantUiControllerAndroid::~AssistantUiControllerAndroid() {}

void AssistantUiControllerAndroid::SetUiDelegate(
    AssistantUiDelegate* ui_delegate) {
  ui_delegate_ = ui_delegate;
}

void AssistantUiControllerAndroid::ShowStatusMessage(
    const std::string& message) {
  JNIEnv* env = AttachCurrentThread();
  Java_AssistantUiController_onShowStatusMessage(
      env, java_assistant_ui_controller_,
      base::android::ConvertUTF8ToJavaString(env, message));
}

void AssistantUiControllerAndroid::ShowOverlay() {
  Java_AssistantUiController_onShowOverlay(AttachCurrentThread(),
                                           java_assistant_ui_controller_);
}

void AssistantUiControllerAndroid::HideOverlay() {
  Java_AssistantUiController_onHideOverlay(AttachCurrentThread(),
                                           java_assistant_ui_controller_);
}

void AssistantUiControllerAndroid::Destroy(JNIEnv* env,
                                           const JavaParamRef<jobject>& obj) {
  ui_delegate_->OnDestroy();
}

static jlong JNI_AssistantUiController_Init(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    const base::android::JavaParamRef<jobject>& webContents) {
  auto* assistant_ui_controller_android =
      new autofill_assistant::AssistantUiControllerAndroid(env, jcaller,
                                                           webContents);
  return reinterpret_cast<intptr_t>(assistant_ui_controller_android);
}

}  // namespace autofill_assistant.
