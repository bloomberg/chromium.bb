// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/autofill_dialog_view_android.h"

#include "base/android/jni_android.h"
#include "content/public/browser/web_contents.h"
#include "jni/AutofillDialogGlue_jni.h"

namespace autofill {

// AutofillDialogView ----------------------------------------------------------

// static
AutofillDialogView* AutofillDialogView::Create(
    AutofillDialogController* controller) {
  return new AutofillDialogViewAndroid(controller);
}

// AutofillDialogViews ---------------------------------------------------------

AutofillDialogViewAndroid::AutofillDialogViewAndroid(
    AutofillDialogController* controller)
    : controller_(controller) {}

AutofillDialogViewAndroid::~AutofillDialogViewAndroid() {}

void AutofillDialogViewAndroid::Show() {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_object_.Reset(Java_AutofillDialogGlue_create(
      env,
      reinterpret_cast<jint>(this),
      base::android::GetApplicationContext()));
}

void AutofillDialogViewAndroid::Hide() {
  NOTIMPLEMENTED();
}

void AutofillDialogViewAndroid::UpdateAccountChooser() {
  NOTIMPLEMENTED();
}

void AutofillDialogViewAndroid::UpdateNotificationArea() {
  NOTIMPLEMENTED();
}

void AutofillDialogViewAndroid::UpdateSection(DialogSection section) {
  NOTIMPLEMENTED();
}

void AutofillDialogViewAndroid::GetUserInput(DialogSection section,
                                             DetailOutputMap* output) {
  NOTIMPLEMENTED();
}

string16 AutofillDialogViewAndroid::GetCvc() {
  NOTIMPLEMENTED();
  return string16();
}

bool AutofillDialogViewAndroid::UseBillingForShipping() {
  NOTIMPLEMENTED();
  return true;
}

bool AutofillDialogViewAndroid::SaveDetailsInWallet() {
  NOTIMPLEMENTED();
  return true;
}

bool AutofillDialogViewAndroid::SaveDetailsLocally() {
  NOTIMPLEMENTED();
  return true;
}

const content::NavigationController& AutofillDialogViewAndroid::ShowSignIn() {
  NOTIMPLEMENTED();
  return controller_->web_contents()->GetController();
}

void AutofillDialogViewAndroid::HideSignIn() {
  NOTIMPLEMENTED();
}

void AutofillDialogViewAndroid::UpdateProgressBar(double value) {
  NOTIMPLEMENTED();
}

void AutofillDialogViewAndroid::ModelChanged() {
  NOTIMPLEMENTED();
}

void AutofillDialogViewAndroid::SubmitForTesting() {
  NOTIMPLEMENTED();
}

void AutofillDialogViewAndroid::CancelForTesting() {
  NOTIMPLEMENTED();
}

// static
bool AutofillDialogViewAndroid::RegisterAutofillDialogViewAndroid(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace autofill
