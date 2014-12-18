// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_AUTOFILL_CARD_UNMASK_PROMPT_VIEW_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_AUTOFILL_CARD_UNMASK_PROMPT_VIEW_ANDROID_H_

#include <jni.h>

#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/strings/string16.h"
#include "chrome/browser/ui/autofill/card_unmask_prompt_view.h"

namespace autofill {

class CardUnmaskPromptViewAndroid : public CardUnmaskPromptView {
 public:
  explicit CardUnmaskPromptViewAndroid(CardUnmaskPromptController* controller);

  void Show();

  void OnUserInput(JNIEnv* env, jobject obj, jstring response);
  void PromptDismissed(JNIEnv* env, jobject obj);

  // CardUnmaskPromptView implementation.
  void ControllerGone() override;
  void DisableAndWaitForVerification() override;
  void VerificationSucceeded() override;
  void VerificationFailed() override;

  static bool Register(JNIEnv* env);

 private:
  virtual ~CardUnmaskPromptViewAndroid();

  // The corresponding java object.
  base::android::ScopedJavaGlobalRef<jobject> java_object_;

  CardUnmaskPromptController* controller_;

  DISALLOW_COPY_AND_ASSIGN(CardUnmaskPromptViewAndroid);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_ANDROID_AUTOFILL_CARD_UNMASK_PROMPT_VIEW_ANDROID_H_
