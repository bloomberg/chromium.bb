// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTOFILL_DIALOG_VIEW_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTOFILL_DIALOG_VIEW_ANDROID_H_

#include <jni.h>

#include "base/android/jni_string.h"
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
  virtual void UpdateNotificationArea() OVERRIDE;
  virtual void UpdateAccountChooser() OVERRIDE;
  virtual void UpdateButtonStrip() OVERRIDE;
  virtual void UpdateSection(DialogSection section) OVERRIDE;
  virtual void FillSection(DialogSection section,
                           const DetailInput& originating_input) OVERRIDE;
  virtual void GetUserInput(DialogSection section,
                            DetailOutputMap* output) OVERRIDE;
  virtual string16 GetCvc() OVERRIDE;
  virtual bool SaveDetailsLocally() OVERRIDE;
  virtual const content::NavigationController* ShowSignIn() OVERRIDE;
  virtual void HideSignIn() OVERRIDE;
  virtual void UpdateProgressBar(double value) OVERRIDE;
  virtual void ModelChanged() OVERRIDE;

  // Java to C++ calls
  void ItemSelected(JNIEnv* env, jobject obj, jint section, jint index);
  void AccountSelected(JNIEnv* env, jobject obj, jint index);
  void EditingStart(JNIEnv* env, jobject obj, jint section);
  jboolean EditingComplete(JNIEnv* env, jobject obj, jint section);
  void EditingCancel(JNIEnv* env, jobject obj, jint section);
  base::android::ScopedJavaLocalRef<jstring> ValidateField(
      JNIEnv* env, jobject obj, jint type, jstring value);
  void ValidateSection(JNIEnv* env, jobject obj, jint section);
  void DialogSubmit(JNIEnv* env, jobject obj);
  void DialogCancel(JNIEnv* env, jobject obj);
  void DialogDismissed(JNIEnv* env, jobject obj);
  base::android::ScopedJavaLocalRef<jstring> GetLabelForSection(
      JNIEnv* env,
      jobject obj,
      jint section);
  base::android::ScopedJavaLocalRef<jobjectArray> GetListForField(JNIEnv* env,
                                                                  jobject obj,
                                                                  jint field);
  void ContinueAutomaticSignin(JNIEnv* env, jobject obj,
                               jstring account_name, jstring sid, jstring lsid);
  base::android::ScopedJavaLocalRef<jobject> GetIconForField(
      JNIEnv* env,
      jobject obj,
      jint field_id,
      jstring jinput);
  base::android::ScopedJavaLocalRef<jstring> GetPlaceholderForField(
      JNIEnv* env,
      jobject obj,
      jint section,
      jint field_id);
  base::android::ScopedJavaLocalRef<jstring> GetDialogButtonText(
      JNIEnv* env,
      jobject obj,
      jint dialog_button_id);
  jboolean IsDialogButtonEnabled(
      JNIEnv* env,
      jobject obj,
      jint dialog_button_id);
  base::android::ScopedJavaLocalRef<jstring> GetSaveLocallyText(JNIEnv* env,
                                                                jobject obj);
  base::android::ScopedJavaLocalRef<jstring> GetProgressBarText(JNIEnv* env,
                                                                jobject obj);

  static bool RegisterAutofillDialogViewAndroid(JNIEnv* env);

 private:
  // Returns the list of available user accounts.
  std::vector<std::string> GetAvailableUserAccounts();
  bool ValidateSection(DialogSection section,
                       AutofillDialogController::ValidationType type);

  // Starts an automatic sign-in attempt for a given account.
  bool StartAutomaticSignIn(const std::string& username);

  // Updates the visibility of the checkbox to save the edited information
  // locally.
  void UpdateSaveLocallyCheckBox();

  // Updates a given |section| to match the state provided by |controller_|. If
  // |clobber_inputs| is true, the user input will be ignored, otherwise it will
  // be preserved for all inputs except for the |field_type_to_always_clobber|.
  void UpdateOrFillSectionToJava(DialogSection section,
                                 bool clobber_inputs,
                                 int field_type_to_always_clobber);

  // Whether the item at the |index| in the |section| menu model is editable.
  bool IsMenuItemEditable(DialogSection section, int index) const;

  // Collapse the user input into a menu item.
  // TODO(aruslan): http://crbug.com/230685
  bool CollapseUserDataIntoMenuItem(DialogSection section,
                                    string16* label,
                                    string16* sublabel,
                                    gfx::Image* icon);

  // The controller that drives this view. Weak pointer, always non-NULL.
  AutofillDialogController* const controller_;

  // The corresponding java object.
  base::android::ScopedJavaGlobalRef<jobject> java_object_;

  DISALLOW_COPY_AND_ASSIGN(AutofillDialogViewAndroid);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTOFILL_DIALOG_VIEW_ANDROID_H_
