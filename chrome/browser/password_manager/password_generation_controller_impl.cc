// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_generation_controller_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/autofill/manual_filling_controller.h"
#include "chrome/browser/password_manager/password_accessory_controller.h"
#include "chrome/browser/password_manager/password_generation_dialog_view_interface.h"
#include "components/autofill/core/common/password_form.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "components/autofill/core/common/signatures_util.h"
#include "components/password_manager/core/browser/password_generation_frame_helper.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/core/browser/password_manager_driver.h"

namespace {

void RecordGenerationDialogDismissal(bool accepted) {
  UMA_HISTOGRAM_BOOLEAN("KeyboardAccessory.GeneratedPasswordDialog", accepted);
}

}  // namespace

PasswordGenerationControllerImpl::~PasswordGenerationControllerImpl() = default;

// static
bool PasswordGenerationController::AllowedForWebContents(
    content::WebContents* web_contents) {
  return PasswordAccessoryController::AllowedForWebContents(web_contents);
}

// static
PasswordGenerationController* PasswordGenerationController::GetOrCreate(
    content::WebContents* web_contents) {
  DCHECK(PasswordGenerationController::AllowedForWebContents(web_contents));

  PasswordGenerationControllerImpl::CreateForWebContents(web_contents);
  return PasswordGenerationControllerImpl::FromWebContents(web_contents);
}

// static
PasswordGenerationController* PasswordGenerationController::GetIfExisting(
    content::WebContents* web_contents) {
  return PasswordGenerationControllerImpl::FromWebContents(web_contents);
}

struct PasswordGenerationControllerImpl::GenerationElementData {
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

void PasswordGenerationControllerImpl::OnAutomaticGenerationAvailable(
    const autofill::password_generation::PasswordGenerationUIData& ui_data,
    const base::WeakPtr<password_manager::PasswordManagerDriver>& driver) {
  target_frame_driver_ = driver;
  generation_element_data_ = std::make_unique<GenerationElementData>(
      ui_data.password_form,
      autofill::CalculateFormSignature(ui_data.password_form.form_data),
      autofill::CalculateFieldSignatureByNameAndType(ui_data.generation_element,
                                                     "password"),
      ui_data.max_length);

  if (!manual_filling_controller_) {
    manual_filling_controller_ =
        ManualFillingController::GetOrCreate(web_contents_);
  }

  DCHECK(manual_filling_controller_);
  manual_filling_controller_->OnAutomaticGenerationStatusChanged(true);
}

void PasswordGenerationControllerImpl::OnGenerationElementLostFocus() {
  if (manual_filling_controller_ && generation_element_data_) {
    manual_filling_controller_->OnAutomaticGenerationStatusChanged(false);
  }
  target_frame_driver_ = nullptr;
  generation_element_data_.reset();
}

void PasswordGenerationControllerImpl::OnGenerationRequested() {
  if (!target_frame_driver_)
    return;
  dialog_view_ = create_dialog_factory_.Run(this);
  uint32_t spec_priority = 0;
  base::string16 password =
      target_frame_driver_->GetPasswordGenerationHelper()->GeneratePassword(
          web_contents_->GetLastCommittedURL().GetOrigin(),
          generation_element_data_->form_signature,
          generation_element_data_->field_signature,
          generation_element_data_->max_password_length, &spec_priority);
  if (target_frame_driver_ && target_frame_driver_->GetPasswordManager()) {
    target_frame_driver_->GetPasswordManager()
        ->ReportSpecPriorityForGeneratedPassword(generation_element_data_->form,
                                                 spec_priority);
  }
  dialog_view_->Show(password, std::move(target_frame_driver_));
  target_frame_driver_ = nullptr;
}

void PasswordGenerationControllerImpl::GeneratedPasswordAccepted(
    const base::string16& password,
    base::WeakPtr<password_manager::PasswordManagerDriver> driver) {
  if (!driver)
    return;
  RecordGenerationDialogDismissal(true);
  driver->GeneratedPasswordAccepted(password);
  dialog_view_.reset();
}

void PasswordGenerationControllerImpl::GeneratedPasswordRejected() {
  RecordGenerationDialogDismissal(false);
  dialog_view_.reset();
}

gfx::NativeWindow PasswordGenerationControllerImpl::top_level_native_window()
    const {
  return web_contents_->GetTopLevelNativeWindow();
}

// static
void PasswordGenerationControllerImpl::CreateForWebContentsForTesting(
    content::WebContents* web_contents,
    base::WeakPtr<ManualFillingController> manual_filling_controller_,
    CreateDialogFactory create_dialog_factory) {
  DCHECK(web_contents) << "Need valid WebContents to attach controller to!";
  DCHECK(!FromWebContents(web_contents)) << "Controller already attached!";
  DCHECK(manual_filling_controller_);

  web_contents->SetUserData(
      UserDataKey(), base::WrapUnique(new PasswordGenerationControllerImpl(
                         web_contents, std::move(manual_filling_controller_),
                         create_dialog_factory)));
}

PasswordGenerationControllerImpl::PasswordGenerationControllerImpl(
    content::WebContents* web_contents)
    : web_contents_(web_contents),
      create_dialog_factory_(
          base::BindRepeating(&PasswordGenerationDialogViewInterface::Create)) {
}

PasswordGenerationControllerImpl::PasswordGenerationControllerImpl(
    content::WebContents* web_contents,
    base::WeakPtr<ManualFillingController> manual_filling_controller_,
    CreateDialogFactory create_dialog_factory)
    : web_contents_(web_contents),
      manual_filling_controller_(std::move(manual_filling_controller_)),
      create_dialog_factory_(create_dialog_factory) {}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PasswordGenerationControllerImpl)
