// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTOFILL_DIALOG_VIEW_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTOFILL_DIALOG_VIEW_ANDROID_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "chrome/browser/ui/autofill/autofill_dialog_controller.h"
#include "chrome/browser/ui/autofill/autofill_dialog_view.h"

namespace autofill {

// Android implementation of the Autofill dialog that handles the imperative
// autocomplete API call.
class AutofillDialogViewAndroid : public AutofillDialogView {
 public:
  explicit AutofillDialogViewAndroid(AutofillDialogController* controller);
  virtual ~AutofillDialogViewAndroid();

  // AutofillDialogView implementation:
  virtual void Show() OVERRIDE;
  virtual void Hide() OVERRIDE;
  virtual void UpdateAccountChooser() OVERRIDE;
  virtual void UpdateNotificationArea() OVERRIDE;
  virtual void UpdateSection(DialogSection section) OVERRIDE;
  virtual void GetUserInput(DialogSection section,
                            DetailOutputMap* output) OVERRIDE;
  virtual string16 GetCvc() OVERRIDE;
  virtual bool UseBillingForShipping() OVERRIDE;
  virtual bool SaveDetailsLocally() OVERRIDE;
  virtual const content::NavigationController& ShowSignIn() OVERRIDE;
  virtual void HideSignIn() OVERRIDE;
  virtual void UpdateProgressBar(double value) OVERRIDE;
  virtual void ModelChanged() OVERRIDE;
  virtual void SubmitForTesting() OVERRIDE;
  virtual void CancelForTesting() OVERRIDE;

  static bool RegisterAutofillDialogViewAndroid(JNIEnv* env);

 private:
  // The controller that drives this view. Weak pointer, always non-NULL.
  AutofillDialogController* const controller_;

  // The corresponding java object.
  base::android::ScopedJavaGlobalRef<jobject> java_object_;

  DISALLOW_COPY_AND_ASSIGN(AutofillDialogViewAndroid);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTOFILL_DIALOG_VIEW_ANDROID_H_
