// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_accessory_controller.h"

#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/android/preferences/preferences_launcher.h"
#include "chrome/browser/password_manager/password_accessory_view_interface.h"
#include "chrome/browser/password_manager/password_generation_dialog_view_interface.h"
#include "chrome/browser/ui/passwords/manage_passwords_view_utils.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/content/browser/content_password_manager_driver.h"
#include "components/password_manager/content/browser/content_password_manager_driver_factory.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

#include "chrome/browser/android/preferences/preferences_launcher.h"

using autofill::PasswordForm;
using Item = PasswordAccessoryViewInterface::AccessoryItem;

DEFINE_WEB_CONTENTS_USER_DATA_KEY(PasswordAccessoryController);

struct PasswordAccessoryController::GenerationElementData {
  GenerationElementData(autofill::PasswordForm form,
                        autofill::FormSignature form_signature,
                        autofill::FieldSignature field_signature,
                        uint32_t max_password_length)
      : form(std::move(form)),
        form_signature(form_signature),
        field_signature(field_signature),
        max_password_length(max_password_length) {}

  // Form for which password generation is triggered.
  autofill::PasswordForm form;

  // Signature of the form for which password generation is triggered.
  autofill::FormSignature form_signature;

  // Signature of the field for which password generation is triggered.
  autofill::FieldSignature field_signature;

  // Maximum length of the generated password.
  uint32_t max_password_length;
};

PasswordAccessoryController::PasswordAccessoryController(
    content::WebContents* web_contents)
    : web_contents_(web_contents),
      view_(PasswordAccessoryViewInterface::Create(this)),
      create_dialog_factory_(
          base::BindRepeating(&PasswordGenerationDialogViewInterface::Create)),
      weak_factory_(this) {}

// Additional creation functions in unit tests only:
PasswordAccessoryController::PasswordAccessoryController(
    content::WebContents* web_contents,
    std::unique_ptr<PasswordAccessoryViewInterface> view,
    CreateDialogFactory create_dialog_factory)
    : web_contents_(web_contents),
      view_(std::move(view)),
      create_dialog_factory_(create_dialog_factory),
      weak_factory_(this) {}

PasswordAccessoryController::~PasswordAccessoryController() = default;

// static
void PasswordAccessoryController::CreateForWebContentsForTesting(
    content::WebContents* web_contents,
    std::unique_ptr<PasswordAccessoryViewInterface> view,
    CreateDialogFactory create_dialog_factory) {
  DCHECK(web_contents) << "Need valid WebContents to attach controller to!";
  DCHECK(!FromWebContents(web_contents)) << "Controller already attached!";
  web_contents->SetUserData(
      UserDataKey(),
      base::WrapUnique(new PasswordAccessoryController(
          web_contents, std::move(view), create_dialog_factory)));
}

void PasswordAccessoryController::OnPasswordsAvailable(
    const std::map<base::string16, const PasswordForm*>& best_matches,
    const GURL& origin) {
  const url::Origin& frame_origin =
      web_contents_->GetFocusedFrame()->GetLastCommittedOrigin();
  url::Origin password_origin = url::Origin::Create(origin);
  if (!frame_origin.IsSameOriginWith(password_origin)) {
    // TODO(fhorschig): Support iframes: https://crbug.com/854150.
    return;  // Don't make passwords available across origins.
  }
  DCHECK(view_);
  std::vector<Item> items;
  base::string16 passwords_title_str;
  passwords_title_str = l10n_util::GetStringFUTF16(
      best_matches.empty()
          ? IDS_PASSWORD_MANAGER_ACCESSORY_PASSWORD_LIST_EMPTY_MESSAGE
          : IDS_PASSWORD_MANAGER_ACCESSORY_PASSWORD_LIST_TITLE,
      base::ASCIIToUTF16(origin.host()));
  items.emplace_back(passwords_title_str, passwords_title_str,
                     /*is_password=*/false, Item::Type::LABEL);
  for (const auto& pair : best_matches) {
    const PasswordForm* form = pair.second;
    base::string16 username = GetDisplayUsername(*form);
    items.emplace_back(username, username, /*is_password=*/false,
                       form->username_value.empty()
                           ? Item::Type::NON_INTERACTIVE_SUGGESTION
                           : Item::Type::SUGGESTION);
    items.emplace_back(
        form->password_value,
        l10n_util::GetStringFUTF16(
            IDS_PASSWORD_MANAGER_ACCESSORY_PASSWORD_DESCRIPTION, username),
        /*is_password=*/true, Item::Type::SUGGESTION);
  }
  items.emplace_back(base::string16(), base::string16(), false,
                     Item::Type::DIVIDER);
  base::string16 manage_passwords_title = l10n_util::GetStringUTF16(
      IDS_PASSWORD_MANAGER_ACCESSORY_ALL_PASSWORDS_LINK);
  items.emplace_back(manage_passwords_title, manage_passwords_title, false,
                     Item::Type::OPTION);
  view_->OnItemsAvailable(origin, items);
}

void PasswordAccessoryController::OnAutomaticGenerationStatusChanged(
    bool available,
    const base::Optional<
        autofill::password_generation::PasswordGenerationUIData>& ui_data,
    const base::WeakPtr<password_manager::PasswordManagerDriver>& driver) {
  target_frame_driver_ = driver;
  if (available) {
    DCHECK(ui_data.has_value());
    generation_element_data_ = std::make_unique<GenerationElementData>(
        ui_data.value().password_form,
        autofill::CalculateFormSignature(
            ui_data.value().password_form.form_data),
        autofill::CalculateFieldSignatureByNameAndType(
            ui_data.value().generation_element, "password"),
        ui_data.value().max_length);
  } else {
    generation_element_data_.reset();
  }
  DCHECK(view_);
  view_->OnAutomaticGenerationStatusChanged(available);
}

void PasswordAccessoryController::OnFillingTriggered(
    bool is_password,
    const base::string16& textToFill) {
  password_manager::ContentPasswordManagerDriverFactory* factory =
      password_manager::ContentPasswordManagerDriverFactory::FromWebContents(
          web_contents_);
  DCHECK(factory);
  // TODO(fhorschig): Consider allowing filling on non-main frames.
  password_manager::ContentPasswordManagerDriver* driver =
      factory->GetDriverForFrame(web_contents_->GetMainFrame());
  if (!driver) {
    return;
  }  // |driver| can be NULL if the tab is being closed.
  driver->FillIntoFocusedField(
      is_password, textToFill,
      base::BindOnce(&PasswordAccessoryController::OnFilledIntoFocusedField,
                     weak_factory_.GetWeakPtr()));
}

void PasswordAccessoryController::OnOptionSelected(
    const base::string16& selectedOption) const {
  if (selectedOption ==
      l10n_util::GetStringUTF16(
          IDS_PASSWORD_MANAGER_ACCESSORY_ALL_PASSWORDS_LINK)) {
    chrome::android::PreferencesLauncher::ShowPasswordSettings();
  }
}

void PasswordAccessoryController::OnGenerationRequested() {
  if (!target_frame_driver_)
    return;
  // TODO(crbug.com/835234): Take the modal dialog logic out of the accessory
  // controller.
  dialog_view_ = create_dialog_factory_.Run(this);
  uint32_t spec_priority = 0;
  base::string16 password =
      target_frame_driver_->GetPasswordGenerationManager()->GeneratePassword(
          web_contents_->GetLastCommittedURL().GetOrigin(),
          generation_element_data_->form_signature,
          generation_element_data_->field_signature,
          generation_element_data_->max_password_length, &spec_priority);
  if (target_frame_driver_ && target_frame_driver_->GetPasswordManager()) {
    target_frame_driver_->GetPasswordManager()
        ->ReportSpecPriorityForGeneratedPassword(generation_element_data_->form,
                                                 spec_priority);
  }
  dialog_view_->Show(password);
}

void PasswordAccessoryController::GeneratedPasswordAccepted(
    const base::string16& password) {
  if (!target_frame_driver_)
    return;
  target_frame_driver_->GeneratedPasswordAccepted(password);
  dialog_view_.reset();
}

void PasswordAccessoryController::GeneratedPasswordRejected() {
  dialog_view_.reset();
}

void PasswordAccessoryController::OnSavedPasswordsLinkClicked() {
  chrome::android::PreferencesLauncher::ShowPasswordSettings();
}

void PasswordAccessoryController::OnFilledIntoFocusedField(
    autofill::FillingStatus status) {
  // TODO(crbug/853766): Record success rate.
  // TODO(fhorschig): Update UI by hiding the sheet or communicating the error.
}

void PasswordAccessoryController::ClearSuggestions() {
  // TODO(fhorschig): This should drop suggestion for the given frame.
  OnPasswordsAvailable(
      /*best_matches=*/{},
      web_contents_->GetFocusedFrame()->GetLastCommittedOrigin().GetURL());
}

void PasswordAccessoryController::RefreshSuggestionsForField(
    bool is_fillable,
    bool is_password_field) {
  // TODO(crbug/853766): Record CTR metric.
  // TODO(fhorschig): Test and implement.
}

gfx::NativeView PasswordAccessoryController::container_view() const {
  return web_contents_->GetNativeView();
}

gfx::NativeWindow PasswordAccessoryController::native_window() const {
  return web_contents_->GetTopLevelNativeWindow();
}
