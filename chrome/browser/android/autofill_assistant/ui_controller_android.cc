// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/autofill_assistant/ui_controller_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "components/autofill_assistant/browser/controller.h"
#include "content/public/browser/web_contents.h"
#include "jni/AutofillAssistantUiController_jni.h"

using base::android::AttachCurrentThread;
using base::android::JavaParamRef;

namespace autofill_assistant {

UiControllerAndroid::UiControllerAndroid(
    JNIEnv* env,
    jobject jcaller,
    const JavaParamRef<jobject>& webContents)
    : ui_delegate_(nullptr) {
  java_autofill_assistant_ui_controller_.Reset(env, jcaller);

  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(webContents);
  DCHECK(web_contents);
  Controller::CreateAndStartForWebContents(web_contents,
                                           base::WrapUnique(this));
  DCHECK(ui_delegate_);
}

UiControllerAndroid::~UiControllerAndroid() {}

void UiControllerAndroid::SetUiDelegate(UiDelegate* ui_delegate) {
  ui_delegate_ = ui_delegate;
}

void UiControllerAndroid::ShowStatusMessage(const std::string& message) {
  JNIEnv* env = AttachCurrentThread();
  Java_AutofillAssistantUiController_onShowStatusMessage(
      env, java_autofill_assistant_ui_controller_,
      base::android::ConvertUTF8ToJavaString(env, message));
}

void UiControllerAndroid::ShowOverlay() {
  Java_AutofillAssistantUiController_onShowOverlay(
      AttachCurrentThread(), java_autofill_assistant_ui_controller_);
}

void UiControllerAndroid::HideOverlay() {
  Java_AutofillAssistantUiController_onHideOverlay(
      AttachCurrentThread(), java_autofill_assistant_ui_controller_);
}

void UiControllerAndroid::ChooseAddress(
    base::OnceCallback<void(const std::string&)> callback) {
  // TODO(crbug.com/806868): Implement ChooseAddress.
  std::move(callback).Run("");
}

void UiControllerAndroid::ChooseCard(
    base::OnceCallback<void(const std::string&)> callback) {
  // TODO(crbug.com/806868): Implement ChooseCard.
  std::move(callback).Run("");
}

void UiControllerAndroid::Destroy(JNIEnv* env,
                                  const JavaParamRef<jobject>& obj) {
  ui_delegate_->OnDestroy();
}

static jlong JNI_AutofillAssistantUiController_Init(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    const base::android::JavaParamRef<jobject>& webContents) {
  auto* ui_controller_android =
      new autofill_assistant::UiControllerAndroid(env, jcaller, webContents);
  return reinterpret_cast<intptr_t>(ui_controller_android);
}

}  // namespace autofill_assistant.
