// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_AUTOFILL_ASSISTANT_ASSISTANT_UI_CONTROLLER_ANDROID_H_
#define CHROME_BROWSER_ANDROID_AUTOFILL_ASSISTANT_ASSISTANT_UI_CONTROLLER_ANDROID_H_

#include "components/autofill_assistant/browser/assistant_ui_controller.h"

#include <string>

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"

namespace autofill_assistant {
// Class implements AssistantUiController and starts AssistantController.
class AssistantUiControllerAndroid : public AssistantUiController {
 public:
  AssistantUiControllerAndroid(
      JNIEnv* env,
      jobject jcaller,
      const base::android::JavaParamRef<jobject>& webContents);
  ~AssistantUiControllerAndroid() override;

  // Overrides AssistantUiController:
  void SetUiDelegate(AssistantUiDelegate* ui_delegate) override;
  void ShowStatusMessage(const std::string& message) override;
  void ShowOverlay() override;
  void HideOverlay() override;

  // Called by Java.
  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);

 private:
  // Java-side AssistantUiController object.
  base::android::ScopedJavaGlobalRef<jobject> java_assistant_ui_controller_;

  AssistantUiDelegate* ui_delegate_;

  DISALLOW_COPY_AND_ASSIGN(AssistantUiControllerAndroid);
};

}  // namespace autofill_assistant.
#endif  // CHROME_BROWSER_ANDROID_AUTOFILL_ASSISTANT_ASSISTANT_UI_CONTROLLER_ANDROID_H_