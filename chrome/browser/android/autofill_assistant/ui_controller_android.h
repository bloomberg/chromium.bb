// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_AUTOFILL_ASSISTANT_UI_CONTROLLER_ANDROID_H_
#define CHROME_BROWSER_ANDROID_AUTOFILL_ASSISTANT_UI_CONTROLLER_ANDROID_H_

#include <string>
#include <vector>

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "components/autofill_assistant/browser/client.h"
#include "components/autofill_assistant/browser/ui_controller.h"

namespace autofill_assistant {
// Class implements UiController, Client and starts the Controller.
class UiControllerAndroid : public UiController, public Client {
 public:
  UiControllerAndroid(
      JNIEnv* env,
      jobject jcaller,
      const base::android::JavaParamRef<jobject>& webContents,
      const base::android::JavaParamRef<jobjectArray>& parameterNames,
      const base::android::JavaParamRef<jobjectArray>& parameterValues);
  ~UiControllerAndroid() override;

  // Overrides UiController:
  void SetUiDelegate(UiDelegate* ui_delegate) override;
  void ShowStatusMessage(const std::string& message) override;
  void ShowOverlay() override;
  void HideOverlay() override;
  void UpdateScripts(const std::vector<ScriptHandle>& scripts) override;
  void ChooseAddress(
      base::OnceCallback<void(const std::string&)> callback) override;
  void ChooseCard(
      base::OnceCallback<void(const std::string&)> callback) override;

  // Overrides Client:
  std::string GetApiKey() override;
  UiController* GetUiController() override;

  // Called by Java.
  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);
  void OnScriptSelected(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller,
      const base::android::JavaParamRef<jstring>& jscript_path);

 private:
  // Java-side AutofillAssistantUiController object.
  base::android::ScopedJavaGlobalRef<jobject>
      java_autofill_assistant_ui_controller_;

  UiDelegate* ui_delegate_;

  DISALLOW_COPY_AND_ASSIGN(UiControllerAndroid);
};

}  // namespace autofill_assistant.
#endif  // CHROME_BROWSER_ANDROID_AUTOFILL_ASSISTANT_UI_CONTROLLER_ANDROID_H_
