// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_INFOBARS_SAVE_PASSWORD_INFOBAR_H_
#define CHROME_BROWSER_UI_ANDROID_INFOBARS_SAVE_PASSWORD_INFOBAR_H_

#include "base/basictypes.h"
#include "base/strings/string16.h"
#include "chrome/browser/password_manager/save_password_infobar_delegate.h"
#include "chrome/browser/ui/android/infobars/confirm_infobar.h"

// InfoBar implementation for saving passwords to be used for autofill.
class SavePasswordInfoBar : public ConfirmInfoBar {
 public:
  explicit SavePasswordInfoBar(
      scoped_ptr<SavePasswordInfoBarDelegate> delegate);
  virtual ~SavePasswordInfoBar();

  void SetUseAdditionalAuthentication(JNIEnv* env,
                                      jobject obj,
                                      bool use_additional_authentication);

 private:
  // ConfirmInfoBar:
  virtual base::android::ScopedJavaLocalRef<jobject> CreateRenderInfoBar(
      JNIEnv* env) OVERRIDE;

  SavePasswordInfoBarDelegate* GetDelegate();

  base::android::ScopedJavaGlobalRef<jobject> java_save_password_delegate_;

  DISALLOW_COPY_AND_ASSIGN(SavePasswordInfoBar);
};

// Registers native methods.
bool RegisterSavePasswordInfoBar(JNIEnv* env);

#endif  // CHROME_BROWSER_UI_ANDROID_INFOBARS_SAVE_PASSWORD_INFOBAR_H_
