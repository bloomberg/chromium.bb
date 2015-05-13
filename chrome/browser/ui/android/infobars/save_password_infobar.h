// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_INFOBARS_SAVE_PASSWORD_INFOBAR_H_
#define CHROME_BROWSER_UI_ANDROID_INFOBARS_SAVE_PASSWORD_INFOBAR_H_

#include "chrome/browser/password_manager/save_password_infobar_delegate.h"
#include "chrome/browser/ui/android/infobars/confirm_infobar.h"

// The Android infobar that offers the user the ability to save a password
// for the site.
class SavePasswordInfoBar : public ConfirmInfoBar {
 public:
  explicit SavePasswordInfoBar(
      scoped_ptr<SavePasswordInfoBarDelegate> delegate);

  ~SavePasswordInfoBar() override;

 static bool Register(JNIEnv* env);

 private:
  // ConfirmInfoBar:
  base::android::ScopedJavaLocalRef<jobject> CreateRenderInfoBar(
      JNIEnv* env) override;
  void OnLinkClicked(JNIEnv* env, jobject obj) override;

  DISALLOW_COPY_AND_ASSIGN(SavePasswordInfoBar);
};


#endif  // CHROME_BROWSER_UI_ANDROID_INFOBARS_SAVE_PASSWORD_INFOBAR_H_
