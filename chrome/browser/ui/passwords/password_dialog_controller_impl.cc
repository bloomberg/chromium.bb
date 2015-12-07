// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/password_dialog_controller_impl.h"

#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/passwords/account_chooser_prompt.h"
#include "chrome/browser/ui/passwords/manage_passwords_view_utils.h"
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

PasswordDialogControllerImpl::PasswordDialogControllerImpl(Profile* profle)
    : profile_(profle),
      current_dialog_(nullptr) {
}

PasswordDialogControllerImpl::~PasswordDialogControllerImpl() = default;

void PasswordDialogControllerImpl::ShowAccountChooser(
    AccountChooserPrompt* dialog,
    std::vector<scoped_ptr<autofill::PasswordForm>> locals,
    std::vector<scoped_ptr<autofill::PasswordForm>> federations) {
  DCHECK(!current_dialog_);
  local_credentials_.swap(locals);
  federated_credentials_.swap(federations);
  current_dialog_ = dialog;
  current_dialog_->Show();
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
  // TODO(vasilii): notify the UI controller.
}

void PasswordDialogControllerImpl::OnChooseCredentials(
    const autofill::PasswordForm& password_form,
    password_manager::CredentialType credential_type) {
  ResetDialog();
  // TODO(vasilii): notify the UI controller.
}

void PasswordDialogControllerImpl::ResetDialog() {
  if (current_dialog_) {
    current_dialog_->ControllerGone();
    current_dialog_ = nullptr;
  }
}
