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
  void NotificationCheckboxStateChanged(JNIEnv* env,
                                        jobject obj,
                                        jint type,
                                        jboolean checked);
  void EditingStart(JNIEnv* env, jobject obj, jint section);
  jboolean EditingComplete(JNIEnv* env, jobject obj, jint section);
  void EditingCancel(JNIEnv* env, jobject obj, jint section);
  void EditedOrActivatedField(JNIEnv* env,
                              jobject obj,
                              jint detail_input,
                              jint view_android,
                              jstring value,
                              jboolean was_edit);
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
  base::android::ScopedJavaLocalRef<jstring> GetLegalDocumentsText(JNIEnv* env,
                                                                   jobject obj);
  base::android::ScopedJavaLocalRef<jstring> GetProgressBarText(JNIEnv* env,
                                                                jobject obj);
  jboolean IsTheAddItem(JNIEnv* env, jobject obj, jint section, jint index);

  static bool RegisterAutofillDialogViewAndroid(JNIEnv* env);

 private:
  // A button type for a menu item.
  enum MenuItemButtonType {
    MENU_ITEM_BUTTON_TYPE_NONE = 0,
    MENU_ITEM_BUTTON_TYPE_ADD = 1,
    MENU_ITEM_BUTTON_TYPE_EDIT = 2,
  };

  // To fit the return type and GetMenuItemButtonType() name into 80 chars.
  typedef MenuItemButtonType MBT;

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

  // Fills |output| with data the user manually input.
  void GetUserInputImpl(DialogSection section, DetailOutputMap* output) const;

  // Returns the model for suggestions for fields that fall under |section|.
  ui::MenuModel* GetMenuModelForSection(DialogSection section) const;

  // Returns the index of the currently selected item in |section|, or -1.
  int GetSelectedItemIndexForSection(DialogSection section) const;

  // Returns true if the item at |index| in |section| is the "Add...".
  bool IsTheAddMenuItem(DialogSection section, int index) const;

  // Returns true if the item at |index| in |section| is the "Manage...".
  bool IsTheManageMenuItem(DialogSection section, int index) const;

  // Returns an |image| converted to a Java image, or null if |image| is empty.
  base::android::ScopedJavaLocalRef<jobject> GetJavaBitmap(
      const gfx::Image& image) const;

  // Returns the button type for a menu item at |index| in |section|.
  MenuItemButtonType GetMenuItemButtonType(
      DialogSection section, int index) const;

  // Collapse the user input into a menu item.
  // TODO(aruslan): http://crbug.com/230685
  bool CollapseUserDataIntoMenuItem(DialogSection section,
                                    string16* label,
                                    string16* sublabel,
                                    gfx::Image* icon) const;

  // The controller that drives this view. Weak pointer, always non-NULL.
  AutofillDialogController* const controller_;

  // The corresponding java object.
  base::android::ScopedJavaGlobalRef<jobject> java_object_;

  DISALLOW_COPY_AND_ASSIGN(AutofillDialogViewAndroid);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTOFILL_DIALOG_VIEW_ANDROID_H_
