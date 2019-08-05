// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_SMS_SMS_DIALOG_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_SMS_SMS_DIALOG_ANDROID_H_

#include "base/android/scoped_java_ref.h"
#include "content/public/browser/sms_dialog.h"

// The SmsDialogAndroid implements the SmsDialog interface to offer users
// the awareness and controls over receiving SMS messages.
class SmsDialogAndroid : public content::SmsDialog {
 public:
  SmsDialogAndroid();
  ~SmsDialogAndroid() override;

  void Open(content::RenderFrameHost*,
            base::OnceClosure on_confirm,
            base::OnceClosure on_cancel) override;
  void Close() override;
  void SmsReceived() override;

  // Report the user manually clicks the 'Confirm' button.
  void OnConfirm(JNIEnv* env);

  // Report the user manually dismisses the SMS dialog.
  void OnCancel(JNIEnv* env);

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_dialog_;
  base::OnceClosure on_confirm_;
  base::OnceClosure on_cancel_;
  DISALLOW_COPY_AND_ASSIGN(SmsDialogAndroid);
};

#endif  // CHROME_BROWSER_UI_ANDROID_SMS_SMS_DIALOG_ANDROID_H_
