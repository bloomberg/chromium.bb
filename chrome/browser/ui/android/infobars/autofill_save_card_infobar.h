// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_INFOBARS_AUTOFILL_SAVE_CARD_INFOBAR_H_
#define CHROME_BROWSER_UI_ANDROID_INFOBARS_AUTOFILL_SAVE_CARD_INFOBAR_H_

#include <jni.h>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/android/infobars/confirm_infobar.h"

namespace autofill {
class AutofillSaveCardInfoBarDelegateMobile;
}

// Android implementation of the infobar for saving credit card information.
class AutofillSaveCardInfoBar : public ConfirmInfoBar {
 public:
  explicit AutofillSaveCardInfoBar(
      scoped_ptr<autofill::AutofillSaveCardInfoBarDelegateMobile> delegate);
  ~AutofillSaveCardInfoBar() override;

  // Called when a link in the legal message text was clicked.
  void OnLegalMessageLinkClicked(JNIEnv* env, jobject obj, jstring url);

  // Registers the JNI bindings.
  static bool Register(JNIEnv* env);

 private:
  // ConfirmInfoBar:
  base::android::ScopedJavaLocalRef<jobject> CreateRenderInfoBar(
      JNIEnv* env) override;

  // Returns the infobar delegate.
  autofill::AutofillSaveCardInfoBarDelegateMobile* GetSaveCardDelegate();

  DISALLOW_COPY_AND_ASSIGN(AutofillSaveCardInfoBar);
};

#endif  // CHROME_BROWSER_UI_ANDROID_INFOBARS_AUTOFILL_SAVE_CARD_INFOBAR_H_
