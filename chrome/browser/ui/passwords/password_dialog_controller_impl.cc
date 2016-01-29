// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/password_dialog_controller_impl.h"

#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/passwords/manage_passwords_view_utils.h"
#include "chrome/browser/ui/passwords/password_dialog_prompts.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "components/autofill/core/common/password_form.h"
#include "components/browser_sync/browser/profile_sync_service.h"
#include "components/password_manager/core/browser/password_bubble_experiment.h"

namespace {
bool IsSmartLockBrandingEnabled(Profile* profile) {
  const ProfileSyncService* sync_service =
      ProfileSyncServiceFactory::GetForProfile(profile);
  return password_bubble_experiment::IsSmartLockBrandingEnabled(sync_service);
}
}  // namespace

PasswordDialogControllerImpl::PasswordDialogControllerImpl(
    Profile* profle,
    PasswordsModelDelegate* delegate)
    : profile_(profle),
      delegate_(delegate),
      account_chooser_dialog_(nullptr),
      autosignin_dialog_(nullptr) {
}

PasswordDialogControllerImpl::~PasswordDialogControllerImpl() {
  ResetDialog();
}

void PasswordDialogControllerImpl::ShowAccountChooser(
    AccountChooserPrompt* dialog,
    std::vector<scoped_ptr<autofill::PasswordForm>> locals,
    std::vector<scoped_ptr<autofill::PasswordForm>> federations) {
  DCHECK(!account_chooser_dialog_);
  DCHECK(!autosignin_dialog_);
  DCHECK(dialog);
  local_credentials_.swap(locals);
  federated_credentials_.swap(federations);
  account_chooser_dialog_ = dialog;
  account_chooser_dialog_->ShowAccountChooser();
}

void PasswordDialogControllerImpl::ShowAutosigninPrompt(
    AutoSigninFirstRunPrompt* dialog) {
  DCHECK(!account_chooser_dialog_);
  DCHECK(!autosignin_dialog_);
  DCHECK(dialog);
  autosignin_dialog_ = dialog;
  autosignin_dialog_->ShowAutoSigninPrompt();
}

const PasswordDialogController::FormsVector&
PasswordDialogControllerImpl::GetLocalForms() const {
  return local_credentials_;
}

const PasswordDialogController::FormsVector&
PasswordDialogControllerImpl::GetFederationsForms() const {
  return federated_credentials_;
}

std::pair<base::string16, gfx::Range>
PasswordDialogControllerImpl::GetAccoutChooserTitle() const {
  std::pair<base::string16, gfx::Range> result;
  GetAccountChooserDialogTitleTextAndLinkRange(
      IsSmartLockBrandingEnabled(profile_),
      &result.first,
      &result.second);
  return result;
}

void PasswordDialogControllerImpl::OnSmartLockLinkClicked() {
  delegate_->NavigateToExternalPasswordManager();
}

void PasswordDialogControllerImpl::OnChooseCredentials(
    const autofill::PasswordForm& password_form,
    password_manager::CredentialType credential_type) {
  ResetDialog();
  delegate_->ChooseCredential(password_form, credential_type);
}

void PasswordDialogControllerImpl::OnCloseAccountChooser() {
  account_chooser_dialog_ = nullptr;
  // The dialog isn't a bubble but ManagePasswordsUIController handles this.
  delegate_->OnBubbleHidden();
}

void PasswordDialogControllerImpl::ResetDialog() {
  if (account_chooser_dialog_) {
    account_chooser_dialog_->ControllerGone();
    account_chooser_dialog_ = nullptr;
  }
  if (autosignin_dialog_) {
    autosignin_dialog_->ControllerGone();
    autosignin_dialog_ = nullptr;
  }
}
