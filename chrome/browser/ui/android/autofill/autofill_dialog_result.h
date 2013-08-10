// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTOFILL_DIALOG_RESULT_H_
#define CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTOFILL_DIALOG_RESULT_H_

#include <jni.h>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"

namespace autofill {
namespace wallet {
class FullWallet;
}
}

namespace autofill {

// A result of the non-cancelled Java-side AutofillDialog invocation.
// Passed to the AutofillDialogControllerAndroid in dialogContinue.
class AutofillDialogResult {
 public:
  // Converts the requested information to wallet::FullWallet.
  // The dialog stores the requested information in the same format
  // regardless if the information was obtained from Autofill or Google Wallet.
  static scoped_ptr<wallet::FullWallet> ConvertFromJava(
      JNIEnv* env, jobject wallet);

  // Returns the email address to be associated with this request,
  // or an empty string.
  static base::string16 GetWalletEmail(JNIEnv* env, jobject wallet);

  // Returns the Google Transaction ID to be associated with this request,
  // or an empty string.
  static std::string GetWalletGoogleTransactionId(JNIEnv* env, jobject wallet);

  // Establishes JNI bindings.
  static bool RegisterAutofillDialogResult(JNIEnv* env);

 private:
  DISALLOW_COPY_AND_ASSIGN(AutofillDialogResult);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTOFILL_DIALOG_RESULT_H_
