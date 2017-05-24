// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_INFOBARS_TRANSLATE_COMPACT_INFOBAR_H_
#define CHROME_BROWSER_UI_ANDROID_INFOBARS_TRANSLATE_COMPACT_INFOBAR_H_

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/ui/android/infobars/infobar_android.h"
#include "components/translate/core/browser/translate_infobar_delegate.h"
#include "components/translate/core/browser/translate_step.h"
#include "components/translate/core/common/translate_errors.h"

namespace translate {
class TranslateInfoBarDelegate;
}

class TranslateCompactInfoBar
    : public InfoBarAndroid,
      public translate::TranslateInfoBarDelegate::Observer {
 public:
  explicit TranslateCompactInfoBar(
      std::unique_ptr<translate::TranslateInfoBarDelegate> delegate);
  ~TranslateCompactInfoBar() override;

  // JNI method specific to string settings in translate.
  void ApplyStringTranslateOption(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      int option,
      const base::android::JavaParamRef<jstring>& value);

  // JNI method specific to boolean settings in translate.
  void ApplyBoolTranslateOption(JNIEnv* env,
                                const base::android::JavaParamRef<jobject>& obj,
                                int option,
                                jboolean value);

  // Check whether we should automatically trigger "Always Translate".
  bool ShouldAutoAlwaysTranslate();

  // Check whether we should automatically trigger "Never Translate Language".
  jboolean ShouldAutoNeverTranslate(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jboolean menu_expanded);

  // TranslateInfoBarDelegate::Observer implementation.
  void OnTranslateStepChanged(translate::TranslateStep step,
                    translate::TranslateErrors::Type error_type) override;
  // Returns true if the user didn't take any affirmative action.
  // The function will be called when the translate infobar is dismissed.
  // If it's true, we will record a declined event.
  bool IsDeclinedByUser() override;

 private:
  // InfoBarAndroid:
  base::android::ScopedJavaLocalRef<jobject> CreateRenderInfoBar(
      JNIEnv* env) override;
  void ProcessButton(int action) override;
  void SetJavaInfoBar(
      const base::android::JavaRef<jobject>& java_info_bar) override;

  translate::TranslateInfoBarDelegate* GetDelegate();

  // Bits for trace user's affirmative actions.
  unsigned int action_flags_;

  // Affirmative action flags to record what the user has done in one session.
  enum ActionFlag {
    FLAG_NONE = 0,
    FLAG_TRANSLATE = 1 << 0,
    FLAG_REVERT = 1 << 1,
    FLAG_ALWAYS_TRANSLATE = 1 << 2,
    FLAG_NEVER_LANGUAGE = 1 << 3,
    FLAG_NEVER_SITE = 1 << 4,
    FLAG_EXPAND_MENU = 1 << 5,
  };

  // If number of consecutive translations is equal to this number, infobar will
  // automatically trigger "Always Translate".
  const int kAcceptCountThreshold = 5;
  // If number of consecutive denied is equal to this number, infobar will
  // automatically trigger "Never Translate Language".
  const int kDeniedCountThreshold = 7;

  DISALLOW_COPY_AND_ASSIGN(TranslateCompactInfoBar);
};

// Registers the native methods through JNI.
bool RegisterTranslateCompactInfoBar(JNIEnv* env);

#endif  // CHROME_BROWSER_UI_ANDROID_INFOBARS_TRANSLATE_COMPACT_INFOBAR_H_
