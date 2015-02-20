// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_INFOBARS_ACCOUNT_CHOOSER_INFOBAR_H_
#define CHROME_BROWSER_UI_ANDROID_INFOBARS_ACCOUNT_CHOOSER_INFOBAR_H_

#include <jni.h>

#include <string>

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "chrome/browser/ui/android/infobars/infobar_android.h"

class AccountChooserInfoBarDelegateAndroid;

// The Android infobar that allows user to choose credentials for login.
class AccountChooserInfoBar : public InfoBarAndroid {
 public:
  explicit AccountChooserInfoBar(
      scoped_ptr<AccountChooserInfoBarDelegateAndroid> delegate);
  ~AccountChooserInfoBar() override;

  void OnCredentialClicked(JNIEnv* env,
                           jobject obj,
                           jint credential_item,
                           jint credential_type);

 private:
  // InfoBarAndroid:
  base::android::ScopedJavaLocalRef<jobject> CreateRenderInfoBar(
      JNIEnv* env) override;
  void ProcessButton(int action, const std::string& action_value) override;
  AccountChooserInfoBarDelegateAndroid* GetDelegate();

  DISALLOW_COPY_AND_ASSIGN(AccountChooserInfoBar);
};

// Registers native methods.
bool RegisterAccountChooserInfoBar(JNIEnv* env);

#endif  // CHROME_BROWSER_UI_ANDROID_INFOBARS_ACCOUNT_CHOOSER_INFOBAR_H_
