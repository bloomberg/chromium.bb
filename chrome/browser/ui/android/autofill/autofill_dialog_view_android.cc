// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/autofill_dialog_view_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/android/window_android_helper.h"
#include "chrome/browser/ui/autofill/data_model_wrapper.h"
#include "components/autofill/browser/autofill_profile.h"
#include "components/autofill/browser/autofill_type.h"
#include "components/autofill/browser/credit_card.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "jni/AutofillDialogGlue_jni.h"
#include "ui/android/window_android.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/combobox_model.h"
#include "ui/base/models/menu_model.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/rect.h"

namespace autofill {

// AutofillDialogView ----------------------------------------------------------

// static
AutofillDialogView* AutofillDialogView::Create(
    AutofillDialogController* controller) {
  return new AutofillDialogViewAndroid(controller);
}

// AutofillDialogView ----------------------------------------------------------

AutofillDialogViewAndroid::AutofillDialogViewAndroid(
    AutofillDialogController* controller)
    : controller_(controller) {}

AutofillDialogViewAndroid::~AutofillDialogViewAndroid() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_AutofillDialogGlue_onDestroy(env, java_object_.obj());
}

void AutofillDialogViewAndroid::Show() {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_object_.Reset(Java_AutofillDialogGlue_create(
      env,
      reinterpret_cast<jint>(this),
      WindowAndroidHelper::FromWebContents(controller_->web_contents())->
          GetWindowAndroid()->GetJavaObject().obj()));
  ModelChanged();
  UpdateNotificationArea();
  UpdateAccountChooser();
}

void AutofillDialogViewAndroid::Hide() {
  JNIEnv* env = base::android::AttachCurrentThread();
  // This will call DialogDismissed(), and that could cause |this| to be
  // deleted by the |controller|.
  Java_AutofillDialogGlue_dismissDialog(env, java_object_.obj());
}

void AutofillDialogViewAndroid::UpdateNotificationArea() {
  JNIEnv* env = base::android::AttachCurrentThread();
  std::vector<DialogNotification> notifications =
      controller_->CurrentNotifications();
  const size_t count = notifications.size();
  ScopedJavaLocalRef<jobjectArray> notification_array =
      Java_AutofillDialogGlue_createAutofillDialogNotificationArray(
          env, count);
  for (size_t i = 0; i < count; ++i) {
    ScopedJavaLocalRef<jstring> text =
        base::android::ConvertUTF16ToJavaString(
            env, notifications[i].display_text());

    Java_AutofillDialogGlue_addToAutofillDialogNotificationArray(
        env,
        notification_array.obj(),
        i,
        static_cast<int>(notifications[i].type()),
        static_cast<int>(notifications[i].GetBackgroundColor()),
        static_cast<int>(notifications[i].GetTextColor()),
        notifications[i].HasArrow(),
        notifications[i].HasCheckbox(),
        notifications[i].checked(),
        notifications[i].interactive(),
        text.obj());
  }

  Java_AutofillDialogGlue_updateNotificationArea(env,
                                                 java_object_.obj(),
                                                 notification_array.obj());
}

void AutofillDialogViewAndroid::UpdateAccountChooser() {
  JNIEnv* env = base::android::AttachCurrentThread();
  std::vector<string16> account_names;
  int selected_account_index = -1;

  ui::MenuModel* model = controller_->MenuModelForAccountChooser();
  if (controller_->ShouldShowSpinner()) {
    // Do not show accounts if not yet known.
  } else if (!model) {
    // TODO(aruslan): http://crbug.com/177495 Publish Android accounts.
    account_names.push_back(controller_->AccountChooserText());
    selected_account_index = 0;
  } else {
    for (int i = 0; i < model->GetItemCount(); ++i) {
      if (model->IsItemCheckedAt(i))
        selected_account_index = i;
      account_names.push_back(model->GetLabelAt(i));
    }
  }

  ScopedJavaLocalRef<jobjectArray> jaccount_names =
      base::android::ToJavaArrayOfStrings(env, account_names);
  Java_AutofillDialogGlue_updateAccountChooser(
      env, java_object_.obj(), jaccount_names.obj(), selected_account_index);
}

void AutofillDialogViewAndroid::UpdateButtonStrip() {
  NOTIMPLEMENTED();
}

void AutofillDialogViewAndroid::UpdateSection(DialogSection section) {
  UpdateOrFillSectionToJava(section, true, UNKNOWN_TYPE);
}

void AutofillDialogViewAndroid::FillSection(
    DialogSection section,
    const DetailInput& originating_input) {
  UpdateOrFillSectionToJava(section, false, originating_input.type);
}

void AutofillDialogViewAndroid::GetUserInput(DialogSection section,
                                             DetailOutputMap* output) {
  GetUserInputImpl(section, output);
}

string16 AutofillDialogViewAndroid::GetCvc() {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> cvc =
      Java_AutofillDialogGlue_getCvc(env, java_object_.obj());
  return base::android::ConvertJavaStringToUTF16(env, cvc.obj());
}

bool AutofillDialogViewAndroid::SaveDetailsLocally() {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_AutofillDialogGlue_shouldSaveDetailsLocally(env,
                                                          java_object_.obj());
}

const content::NavigationController* AutofillDialogViewAndroid::ShowSignIn() {
  NOTIMPLEMENTED();
  return NULL;
}

void AutofillDialogViewAndroid::HideSignIn() {
  NOTIMPLEMENTED();
}

// TODO(aruslan): bind to the automatic sign-in.
bool AutofillDialogViewAndroid::StartAutomaticSignIn(
    const std::string& username) {
  if (username.empty())
    return false;
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> jusername =
      base::android::ConvertUTF8ToJavaString(env, username);
  Java_AutofillDialogGlue_startAutomaticSignIn(env, java_object_.obj(),
                                               jusername.obj());
  return true;
}

void AutofillDialogViewAndroid::UpdateProgressBar(double value) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_AutofillDialogGlue_updateProgressBar(env, java_object_.obj(), value);
}

void AutofillDialogViewAndroid::ModelChanged() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_AutofillDialogGlue_modelChanged(
      env, java_object_.obj(),
      controller_->ShouldShowSpinner());
  UpdateSection(SECTION_EMAIL);
  UpdateSection(SECTION_CC);
  UpdateSection(SECTION_BILLING);
  UpdateSection(SECTION_CC_BILLING);
  UpdateSection(SECTION_SHIPPING);
}

// TODO(aruslan): bind to the list of accounts population.
std::vector<std::string> AutofillDialogViewAndroid::GetAvailableUserAccounts() {
  std::vector<std::string> account_names;
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobjectArray> jaccount_names =
      Java_AutofillDialogGlue_getUserAccountNames(env, java_object_.obj());
  base::android::AppendJavaStringArrayToStringVector(
      env, jaccount_names.obj(), &account_names);
  return account_names;
}

// Calls from Java to C++

void AutofillDialogViewAndroid::ItemSelected(JNIEnv* env, jobject obj,
                                             jint section, jint index) {
  ui::MenuModel* menuModel =
      GetMenuModelForSection(static_cast<DialogSection>(section));
  if (menuModel)
    menuModel->ActivatedAt(index);
}

ScopedJavaLocalRef<jobject> AutofillDialogViewAndroid::GetIconForField(
    JNIEnv* env,
    jobject obj,
    jint field_id,
    jstring jinput) {
  string16 input = base::android::ConvertJavaStringToUTF16(env, jinput);
  return GetJavaBitmap(controller_->IconForField(
      static_cast<AutofillFieldType>(field_id), input));
}

ScopedJavaLocalRef<jstring> AutofillDialogViewAndroid::GetPlaceholderForField(
    JNIEnv* env,
    jobject obj,
    jint section,
    jint field_id) {
  if (static_cast<int>(field_id) == CREDIT_CARD_VERIFICATION_CODE) {
    return base::android::ConvertUTF16ToJavaString(
        env,
        l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_PLACEHOLDER_CVC));
  }
  return ScopedJavaLocalRef<jstring>();
}

ScopedJavaLocalRef<jstring> AutofillDialogViewAndroid::GetDialogButtonText(
    JNIEnv* env,
    jobject obj,
    jint dialog_button_id) {
  switch (static_cast<ui::DialogButton>(dialog_button_id)) {
    case ui::DIALOG_BUTTON_OK:
      return base::android::ConvertUTF16ToJavaString(
          env,
          controller_->ConfirmButtonText());

    case ui::DIALOG_BUTTON_CANCEL:
      return base::android::ConvertUTF16ToJavaString(
          env,
          controller_->CancelButtonText());

    case ui::DIALOG_BUTTON_NONE:
      break;
  }

  NOTREACHED();
  return ScopedJavaLocalRef<jstring>();
}

jboolean AutofillDialogViewAndroid::IsDialogButtonEnabled(
    JNIEnv* env,
    jobject obj,
    jint dialog_button_id) {
  return static_cast<jboolean>(
      controller_->IsDialogButtonEnabled(
          static_cast<ui::DialogButton>(dialog_button_id)));
}

ScopedJavaLocalRef<jstring> AutofillDialogViewAndroid::GetSaveLocallyText(
    JNIEnv* env,
    jobject obj) {
  return base::android::ConvertUTF16ToJavaString(
      env,
      controller_->SaveLocallyText());
}

ScopedJavaLocalRef<jstring> AutofillDialogViewAndroid::GetLegalDocumentsText(
    JNIEnv* env,
    jobject obj) {
  return base::android::ConvertUTF16ToJavaString(
      env,
      controller_->LegalDocumentsText());
}

ScopedJavaLocalRef<jstring> AutofillDialogViewAndroid::GetProgressBarText(
    JNIEnv* env,
    jobject obj) {
  return base::android::ConvertUTF16ToJavaString(
      env,
      controller_->ProgressBarText());
}

jboolean AutofillDialogViewAndroid::IsTheAddItem(
    JNIEnv* env,
    jobject obj,
    jint section,
    jint index) {
  return IsTheAddMenuItem(static_cast<DialogSection>(section),
                          static_cast<int>(index));
}

void AutofillDialogViewAndroid::AccountSelected(JNIEnv* env, jobject obj,
                                                jint index) {
  ui::MenuModel* model = controller_->MenuModelForAccountChooser();
  if (!model)
    return;

  model->ActivatedAt(index);
}

void AutofillDialogViewAndroid::NotificationCheckboxStateChanged(
    JNIEnv* env,
    jobject obj,
    jint type,
    jboolean checked) {
  controller_->NotificationCheckboxStateChanged(
      static_cast<DialogNotification::Type>(type),
      static_cast<bool>(checked));
}

void AutofillDialogViewAndroid::EditingStart(JNIEnv* env, jobject obj,
                                             jint jsection) {
  const DialogSection section = static_cast<DialogSection>(jsection);
  // TODO(estade): respect |suggestion_state.text_style|.
  const SuggestionState& suggestion_state =
      controller_->SuggestionStateForSection(section);
  if (suggestion_state.text.empty()) {
    // The section is already in the editing mode, or it's the "Add...".
    return;
  }

  controller_->EditClickedForSection(section);
  UpdateOrFillSectionToJava(section, false, UNKNOWN_TYPE);
}

jboolean AutofillDialogViewAndroid::EditingComplete(JNIEnv* env,
                                                    jobject obj,
                                                    jint jsection) {
  // Unfortunately, edits are not sent to the models, http://crbug.com/223919.
  const DialogSection section = static_cast<DialogSection>(jsection);
  if (ValidateSection(section, AutofillDialogController::VALIDATE_FINAL)) {
    UpdateOrFillSectionToJava(section, false, UNKNOWN_TYPE);
    return true;
  }

  return false;
}

void AutofillDialogViewAndroid::EditingCancel(JNIEnv* env,
                                              jobject obj,
                                              jint jsection) {
  const DialogSection section = static_cast<DialogSection>(jsection);
  controller_->EditCancelledForSection(section);
  UpdateSection(section);
}

void AutofillDialogViewAndroid::EditedOrActivatedField(JNIEnv* env,
                                                       jobject obj,
                                                       jint detail_input,
                                                       jint view_android,
                                                       jstring value,
                                                       jboolean was_edit) {
  DetailInput* input = reinterpret_cast<DetailInput*>(detail_input);
  ui::ViewAndroid* view = reinterpret_cast<ui::ViewAndroid*>(view_android);
  gfx::Rect rect = gfx::Rect(0, 0, 0, 0);
  string16 value16 = base::android::ConvertJavaStringToUTF16(
      env, value);
  controller_->UserEditedOrActivatedInput(input, view, rect, value16, was_edit);
}

ScopedJavaLocalRef<jstring> AutofillDialogViewAndroid::ValidateField(
    JNIEnv* env,
    jobject obj,
    jint type,
    jstring value) {
  string16 field_value = base::android::ConvertJavaStringToUTF16(env, value);
  AutofillFieldType field_type = static_cast<AutofillFieldType>(type);
  if (controller_->InputIsValid(field_type, field_value)) {
    return ScopedJavaLocalRef<jstring>();
  } else {
    // TODO(aurimas) Start using real error strings.
    string16 error = ASCIIToUTF16("Error");
    ScopedJavaLocalRef<jstring> error_text =
        base::android::ConvertUTF16ToJavaString(env, error);
    return error_text;
  }
}

void AutofillDialogViewAndroid::ValidateSection(JNIEnv* env,
                                                jobject obj,
                                                jint section) {
  ValidateSection(static_cast<DialogSection>(section),
                  AutofillDialogController::VALIDATE_EDIT);
}

void AutofillDialogViewAndroid::DialogSubmit(JNIEnv* env, jobject obj) {
  // TODO(aurimas): add validation step.
  controller_->OnAccept();
}

void AutofillDialogViewAndroid::DialogCancel(JNIEnv* env, jobject obj) {
  controller_->OnCancel();
  Hide();
}

void AutofillDialogViewAndroid::DialogDismissed(JNIEnv* env, jobject obj) {
  // This might delete |this|, as |this| is owned by the |controller_|.
  controller_->ViewClosed();
}

ScopedJavaLocalRef<jstring> AutofillDialogViewAndroid::GetLabelForSection(
    JNIEnv* env,
    jobject obj,
    jint section) {
  string16 label(controller_->LabelForSection(
      static_cast<DialogSection>(section)));
  return base::android::ConvertUTF16ToJavaString(env, label);
}

ScopedJavaLocalRef<jobjectArray> AutofillDialogViewAndroid::GetListForField(
    JNIEnv* env,
    jobject obj,
    jint field) {
  ui::ComboboxModel* model = controller_->ComboboxModelForAutofillType(
      static_cast<AutofillFieldType>(field));
  if (!model)
    return ScopedJavaLocalRef<jobjectArray>();

  std::vector<string16> list(model->GetItemCount());
  for (int i = 0; i < model->GetItemCount(); ++i) {
    list[i] = model->GetItemAt(i);
  }
  return base::android::ToJavaArrayOfStrings(env, list);
}

void AutofillDialogViewAndroid::ContinueAutomaticSignin(
    JNIEnv* env, jobject obj,
    jstring jaccount_name, jstring jsid, jstring jlsid) {
  const std::string account_name =
      base::android::ConvertJavaStringToUTF8(env, jaccount_name);
  const std::string sid =
      base::android::ConvertJavaStringToUTF8(env, jsid);
  const std::string lsid =
      base::android::ConvertJavaStringToUTF8(env, jlsid);
  // TODO(aruslan): bind to the automatic sign-in.
  // controller_->ContinueAutomaticSignIn(account_name, sid, lsid);
}

// static
bool AutofillDialogViewAndroid::RegisterAutofillDialogViewAndroid(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

bool AutofillDialogViewAndroid::ValidateSection(
    DialogSection section, AutofillDialogController::ValidationType type) {
  DetailOutputMap detail_outputs;
  GetUserInput(section, &detail_outputs);
  ValidityData invalid_inputs =
      controller_->InputsAreValid(detail_outputs, type);

  const size_t item_count = invalid_inputs.size();
  if (item_count == 0) return true;

  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobjectArray> error_array =
      Java_AutofillDialogGlue_createAutofillDialogFieldError(env, item_count);
  size_t i = 0;
  for (ValidityData::const_iterator iter =
           invalid_inputs.begin(); iter != invalid_inputs.end(); ++iter, ++i) {
    ScopedJavaLocalRef<jstring> error_text =
        base::android::ConvertUTF16ToJavaString(env, iter->second);
    Java_AutofillDialogGlue_addToAutofillDialogFieldErrorArray(
        env, error_array.obj(), i, iter->first, error_text.obj());
  }
  Java_AutofillDialogGlue_updateSectionErrors(env,
                                              java_object_.obj(),
                                              section,
                                              error_array.obj());
  return false;
}

void AutofillDialogViewAndroid::UpdateSaveLocallyCheckBox() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_AutofillDialogGlue_updateSaveLocallyCheckBox(
      env, java_object_.obj(), controller_->ShouldOfferToSaveInChrome());
}

void AutofillDialogViewAndroid::UpdateOrFillSectionToJava(
    DialogSection section,
    bool clobber_inputs,
    int field_type_to_always_clobber) {
  JNIEnv* env = base::android::AttachCurrentThread();
  const DetailInputs& updated_inputs =
      controller_->RequestedFieldsForSection(section);

  const size_t input_count = updated_inputs.size();
  ScopedJavaLocalRef<jobjectArray> field_array =
      Java_AutofillDialogGlue_createAutofillDialogFieldArray(
          env, input_count);

  for (size_t i = 0; i < input_count; ++i) {
    const DetailInput& input = updated_inputs[i];

    ScopedJavaLocalRef<jstring> autofilled =
        base::android::ConvertUTF16ToJavaString(env, input.initial_value);

    string16 placeholder16;
    if (input.placeholder_text_rid > 0)
      placeholder16 = l10n_util::GetStringUTF16(input.placeholder_text_rid);
    ScopedJavaLocalRef<jstring> placeholder =
        base::android::ConvertUTF16ToJavaString(env, placeholder16);

    Java_AutofillDialogGlue_addToAutofillDialogFieldArray(
        env,
        field_array.obj(),
        i,
        reinterpret_cast<jint>(&input),
        input.type,
        placeholder.obj(),
        autofilled.obj());
  }

  ui::MenuModel* menu_model = GetMenuModelForSection(section);
  const int item_count = menu_model->GetItemCount();

  // Never show the "Manage..." item.
  // TODO(aruslan): fix this once we have UI design http://crbug.com/235639.
  const int jitem_count = item_count >= 1 ? item_count - 1 : 0;

  ScopedJavaLocalRef<jobjectArray> menu_array =
      Java_AutofillDialogGlue_createAutofillDialogMenuItemArray(env,
                                                                jitem_count);
  const int selected_item = GetSelectedItemIndexForSection(section);

  for (int i = 0; i < jitem_count; ++i) {
    const MenuItemButtonType button_type = GetMenuItemButtonType(section, i);
    string16 line1_value = menu_model->GetLabelAt(i);
    string16 line2_value = menu_model->GetSublabelAt(i);
    gfx::Image icon_value;
    menu_model->GetIconAt(i, &icon_value);

    if (i == selected_item) {
      CollapseUserDataIntoMenuItem(section,
                                   &line1_value, &line2_value,
                                   &icon_value);
    }

    ScopedJavaLocalRef<jstring> line1 =
        base::android::ConvertUTF16ToJavaString(env, line1_value);
    ScopedJavaLocalRef<jstring> line2 =
        base::android::ConvertUTF16ToJavaString(env, line2_value);
    ScopedJavaLocalRef<jobject> icon = GetJavaBitmap(icon_value);

    Java_AutofillDialogGlue_addToAutofillDialogMenuItemArray(
        env, menu_array.obj(), i,
        line1.obj(), line2.obj(), icon.obj(),
        static_cast<int>(button_type));
  }

  Java_AutofillDialogGlue_updateSection(env,
                                        java_object_.obj(),
                                        section,
                                        controller_->SectionIsActive(section),
                                        field_array.obj(),
                                        menu_array.obj(),
                                        selected_item,
                                        clobber_inputs,
                                        field_type_to_always_clobber);

  UpdateSaveLocallyCheckBox();
}

void AutofillDialogViewAndroid::GetUserInputImpl(
    DialogSection section,
    DetailOutputMap* output) const {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobjectArray> fields =
      Java_AutofillDialogGlue_getSection(env, java_object_.obj(), section);
  if (fields.is_null())
    return;

  const int arrayLength = env->GetArrayLength(fields.obj());
  for (int i = 0; i < arrayLength; ++i) {
    ScopedJavaLocalRef<jobject> field(
        env, env->GetObjectArrayElement(fields.obj(), i));
    DetailInput* input = reinterpret_cast<DetailInput*>(
        Java_AutofillDialogGlue_getFieldNativePointer(env, field.obj()));
    string16 value = base::android::ConvertJavaStringToUTF16(
        env, Java_AutofillDialogGlue_getFieldValue(env, field.obj()).obj());
    output->insert(std::make_pair(input, value));
  }
}

ui::MenuModel* AutofillDialogViewAndroid::GetMenuModelForSection(
    DialogSection section) const {
  // TODO(aruslan/estade): This should use MenuModelForSectionHack(), which
  // never returns NULL.  Later the controller should return a proper platform-
  // specific menu model to match the platform UI flow (or it shouldn't attempt
  // to control the presentation details).
  return controller_->MenuModelForSectionHack(section);
}

// Returns the index of the currently selected item in |section|, or -1.
int AutofillDialogViewAndroid::GetSelectedItemIndexForSection(
    DialogSection section) const {
  ui::MenuModel* menuModel = GetMenuModelForSection(section);
  if (!menuModel) return -1;

  for (int i = 0; i < menuModel->GetItemCount(); ++i) {
    if (menuModel->IsItemCheckedAt(i))
      return i;
  }

  return -1;
}

// Returns true if the item at |index| in |section| is the "Add...".
bool AutofillDialogViewAndroid::IsTheAddMenuItem(
    DialogSection section, int index) const {
  ui::MenuModel* menuModel = GetMenuModelForSection(section);
  return menuModel && index == menuModel->GetItemCount() - 2;
}

// Returns true if the item at |index| in |section| is the "Manage...".
bool AutofillDialogViewAndroid::IsTheManageMenuItem(
    DialogSection section, int index) const {
  ui::MenuModel* menuModel = GetMenuModelForSection(section);
  return menuModel && index == menuModel->GetItemCount() - 1;
}

// Returns an |image| converted to a Java image, or null if |image| is empty.
ScopedJavaLocalRef<jobject> AutofillDialogViewAndroid::GetJavaBitmap(
    const gfx::Image& image) const {
  ScopedJavaLocalRef<jobject> bitmap;
  const SkBitmap& sk_bitmap = image.AsBitmap();
  if (!sk_bitmap.isNull() && sk_bitmap.bytesPerPixel() != 0)
    bitmap = gfx::ConvertToJavaBitmap(&sk_bitmap);
  return bitmap;
}

// Returns a MenuItemButtonType for a menu item at |index| in |section|.
AutofillDialogViewAndroid::MBT AutofillDialogViewAndroid::GetMenuItemButtonType(
    DialogSection section,
    int index) const {
  // TODO(aruslan): Remove/fix this once http://crbug.com/224162 is closed.
  ui::MenuModel* menu_model = GetMenuModelForSection(section);
  if (!menu_model)
    return MENU_ITEM_BUTTON_TYPE_NONE;

  // "Use billing for shipping" is not editable and it's always first.
  if (section == SECTION_SHIPPING && index == 0)
    return MENU_ITEM_BUTTON_TYPE_NONE;

  // The last item ("Manage...") is never editable.
  if (IsTheManageMenuItem(section, index))
    return MENU_ITEM_BUTTON_TYPE_NONE;

  // The second last item ("Add..."):
  if (IsTheAddMenuItem(section, index)) {
    const bool currently_selected =
        index == GetSelectedItemIndexForSection(section);
    // - shouldn't show any button if not selected;
    if (!currently_selected)
      return MENU_ITEM_BUTTON_TYPE_NONE;

    // - should show "Add..." button if selected and doesn't have user data;
    string16 label, sublabel;
    gfx::Image icon;
    const bool has_user_data =
        CollapseUserDataIntoMenuItem(section, &label, &sublabel, &icon);
    if (!has_user_data)
      return MENU_ITEM_BUTTON_TYPE_ADD;

    // - otherwise, should show the "Edit..." button (as any "normal" items).
  }

  return MENU_ITEM_BUTTON_TYPE_EDIT;
}

// TODO(aruslan): Remove/fix this once http://crbug.com/230685 is closed.
namespace {

bool IsCreditCardType(AutofillFieldType type) {
  return AutofillType(type).group() == AutofillType::CREDIT_CARD;
}

class CollapsedAutofillProfileWrapper : public AutofillProfileWrapper {
 public:
  explicit CollapsedAutofillProfileWrapper(const AutofillProfile* profile)
    : AutofillProfileWrapper(profile, 0) {
  }
  virtual ~CollapsedAutofillProfileWrapper() {
  }

  virtual string16 GetDisplayText() OVERRIDE {
    const string16 name_full = GetInfo(NAME_FULL);
    const string16 comma = ASCIIToUTF16(", ");
    const string16 address2 = GetInfo(ADDRESS_HOME_LINE2);

    string16 label;
    if (!name_full.empty())
      label = name_full + comma;
    label += GetInfo(ADDRESS_HOME_LINE1);
    if (!address2.empty())
      label += comma + address2;
    return label;
  }

  string16 GetSublabel() {
    const string16 comma = ASCIIToUTF16(", ");
    return
        GetInfo(ADDRESS_HOME_CITY) + comma +
        GetInfo(ADDRESS_HOME_STATE) + ASCIIToUTF16(" ") +
        GetInfo(ADDRESS_HOME_ZIP);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CollapsedAutofillProfileWrapper);
};


// Get billing info from |output| and put it into |card|, |cvc|, and |profile|.
// These outparams are required because |card|/|profile| accept different types
// of raw info.
void GetBillingInfoFromOutputs(const DetailOutputMap& output,
                               CreditCard* card,
                               AutofillProfile* profile) {
  for (DetailOutputMap::const_iterator it = output.begin();
       it != output.end(); ++it) {
    string16 trimmed;
    TrimWhitespace(it->second, TRIM_ALL, &trimmed);

    if (it->first->type == ADDRESS_HOME_COUNTRY ||
        it->first->type == ADDRESS_BILLING_COUNTRY) {
        profile->SetInfo(it->first->type,
                         trimmed,
                         g_browser_process->GetApplicationLocale());
    } else {
      // Copy the credit card name to |profile| in addition to |card| as
      // wallet::Instrument requires a recipient name for its billing address.
      if (profile && it->first->type == CREDIT_CARD_NAME)
        profile->SetRawInfo(NAME_FULL, trimmed);

      if (IsCreditCardType(it->first->type)) {
        if (card)
          card->SetRawInfo(it->first->type, trimmed);
      } else if (profile) {
        profile->SetRawInfo(it->first->type, trimmed);
      }
    }
  }
}

}  // namespace

// Returns true and fills in the |label|, |sublabel| and |icon if
// a given |section| has the user input.
// TODO(aruslan): Remove/fix this once http://crbug.com/230685 is closed.
bool AutofillDialogViewAndroid::CollapseUserDataIntoMenuItem(
    DialogSection section,
    string16* label_to_set,
    string16* sublabel_to_set,
    gfx::Image* icon_to_set) const {
  const SuggestionState& suggestion_state =
      controller_->SuggestionStateForSection(section);
  if (!suggestion_state.text.empty())
    return false;

  DetailOutputMap inputs;
  GetUserInputImpl(section, &inputs);

  string16 label;
  string16 sublabel;
  gfx::Image icon;

  switch (section) {
    case SECTION_CC: {
      CreditCard card;
      GetBillingInfoFromOutputs(inputs, &card, NULL);
      AutofillCreditCardWrapper ccw(&card);
      label = card.Label();
      icon = ccw.GetIcon();
      break;
    }

    case SECTION_BILLING: {
      AutofillProfile profile;
      GetBillingInfoFromOutputs(inputs, NULL, &profile);
      CollapsedAutofillProfileWrapper pw(&profile);
      label = pw.GetDisplayText();
      sublabel = pw.GetSublabel();
      break;
    }

    case SECTION_CC_BILLING: {
      CreditCard card;
      AutofillProfile profile;
      GetBillingInfoFromOutputs(inputs, &card, &profile);
      AutofillCreditCardWrapper ccw(&card);
      CollapsedAutofillProfileWrapper pw(&profile);
      label = ccw.GetDisplayText();
      sublabel = pw.GetDisplayText();
      icon = ccw.GetIcon();
      break;
    }

    case SECTION_SHIPPING: {
      AutofillProfile profile;
      GetBillingInfoFromOutputs(inputs, NULL, &profile);
      CollapsedAutofillProfileWrapper pw(&profile);
      label = pw.GetDisplayText();
      sublabel = pw.GetSublabel();
      break;
    }

    case SECTION_EMAIL: {
      for (DetailOutputMap::const_iterator iter = inputs.begin();
           iter != inputs.end(); ++iter) {
        if (iter->first->type == EMAIL_ADDRESS)
          label = iter->second;
      }
      break;
    }
  }

  if (label.empty())
    return false;

  *label_to_set = label;
  *sublabel_to_set = sublabel;
  *icon_to_set = icon;
  return true;
}
}  // namespace autofill
