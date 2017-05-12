// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_INFOBARS_TRANSLATE_COMPACT_INFOBAR_H_
#define CHROME_BROWSER_UI_ANDROID_INFOBARS_TRANSLATE_COMPACT_INFOBAR_H_

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/ui/android/infobars/infobar_android.h"
#include "components/translate/content/browser/content_translate_driver.h"

namespace translate {
class TranslateInfoBarDelegate;
}

class TranslateCompactInfoBar
    : public InfoBarAndroid,
      public translate::ContentTranslateDriver::Observer {
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

  // ContentTranslateDriver::Observer implementation.
  void OnPageTranslated(const std::string& original_lang,
                        const std::string& translated_lang,
                        translate::TranslateErrors::Type error_type) override;

 private:
  // InfoBarAndroid:
  base::android::ScopedJavaLocalRef<jobject> CreateRenderInfoBar(
      JNIEnv* env) override;
  void ProcessButton(int action) override;
  void SetJavaInfoBar(
      const base::android::JavaRef<jobject>& java_info_bar) override;

  translate::TranslateInfoBarDelegate* GetDelegate();
  translate::ContentTranslateDriver* translate_driver_;

  DISALLOW_COPY_AND_ASSIGN(TranslateCompactInfoBar);
};

// Registers the native methods through JNI.
bool RegisterTranslateCompactInfoBar(JNIEnv* env);

#endif  // CHROME_BROWSER_UI_ANDROID_INFOBARS_TRANSLATE_COMPACT_INFOBAR_H_
