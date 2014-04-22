// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/manage_passwords_bubble_model.h"

#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/passwords/manage_passwords_bubble_ui_controller.h"
#include "components/password_manager/core/browser/password_store.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

using content::WebContents;
using autofill::PasswordFormMap;

ManagePasswordsBubbleModel::ManagePasswordsBubbleModel(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      web_contents_(web_contents),
      display_disposition_(
          password_manager::metrics_util::AUTOMATIC_WITH_PASSWORD_PENDING),
      dismissal_reason_(password_manager::metrics_util::NOT_DISPLAYED) {
  ManagePasswordsBubbleUIController* manage_passwords_bubble_ui_controller =
      ManagePasswordsBubbleUIController::FromWebContents(web_contents_);

  if (manage_passwords_bubble_ui_controller->password_to_be_saved())
    manage_passwords_bubble_state_ = PASSWORD_TO_BE_SAVED;
  else
    manage_passwords_bubble_state_ = MANAGE_PASSWORDS;

  title_ = l10n_util::GetStringUTF16(
      (manage_passwords_bubble_state_ == PASSWORD_TO_BE_SAVED) ?
          IDS_SAVE_PASSWORD : IDS_MANAGE_PASSWORDS);
  pending_credentials_ =
      manage_passwords_bubble_ui_controller->PendingCredentials();
  best_matches_ = manage_passwords_bubble_ui_controller->best_matches();
  manage_link_ =
      l10n_util::GetStringUTF16(IDS_OPTIONS_PASSWORDS_MANAGE_PASSWORDS_LINK);
}

ManagePasswordsBubbleModel::~ManagePasswordsBubbleModel() {}

void ManagePasswordsBubbleModel::OnBubbleShown(
    ManagePasswordsBubble::DisplayReason reason) {
  DCHECK(WaitingToSavePassword() ||
         reason == ManagePasswordsBubble::USER_ACTION);
  if (reason == ManagePasswordsBubble::USER_ACTION) {
    if (WaitingToSavePassword()) {
      display_disposition_ =
          password_manager::metrics_util::MANUAL_WITH_PASSWORD_PENDING;
    } else {
      // TODO(mkwst): Deal with "Never save passwords" once we've decided how
      // that flow should work.
      display_disposition_ =
          password_manager::metrics_util::MANUAL_MANAGE_PASSWORDS;
    }
  } else {
    display_disposition_ =
        password_manager::metrics_util::AUTOMATIC_WITH_PASSWORD_PENDING;
  }
  password_manager::metrics_util::LogUIDisplayDisposition(display_disposition_);

  // Default to a dismissal reason of "no interaction". If the user interacts
  // with the button in such a way that it closes, we'll reset this value
  // accordingly.
  dismissal_reason_ = password_manager::metrics_util::NO_DIRECT_INTERACTION;
}

void ManagePasswordsBubbleModel::OnBubbleHidden() {
  if (dismissal_reason_ == password_manager::metrics_util::NOT_DISPLAYED)
    return;

  password_manager::metrics_util::LogUIDismissalReason(dismissal_reason_);
}

void ManagePasswordsBubbleModel::OnCloseWithoutLogging() {
  dismissal_reason_ = password_manager::metrics_util::NOT_DISPLAYED;
}

void ManagePasswordsBubbleModel::OnNopeClicked() {
  dismissal_reason_ = password_manager::metrics_util::CLICKED_NOPE;
  manage_passwords_bubble_state_ = PASSWORD_TO_BE_SAVED;
}

void ManagePasswordsBubbleModel::OnNeverForThisSiteClicked() {
  dismissal_reason_ = password_manager::metrics_util::CLICKED_NEVER;
  ManagePasswordsBubbleUIController* manage_passwords_bubble_ui_controller =
      ManagePasswordsBubbleUIController::FromWebContents(web_contents_);
  manage_passwords_bubble_ui_controller->NeverSavePassword();
  manage_passwords_bubble_ui_controller->unset_password_to_be_saved();
  manage_passwords_bubble_state_ = NEVER_SAVE_PASSWORDS;
}

void ManagePasswordsBubbleModel::OnSaveClicked() {
  dismissal_reason_ = password_manager::metrics_util::CLICKED_SAVE;
  ManagePasswordsBubbleUIController* manage_passwords_bubble_ui_controller =
      ManagePasswordsBubbleUIController::FromWebContents(web_contents_);
  manage_passwords_bubble_ui_controller->SavePassword();
  manage_passwords_bubble_ui_controller->unset_password_to_be_saved();
  manage_passwords_bubble_state_ = MANAGE_PASSWORDS;
}

void ManagePasswordsBubbleModel::OnDoneClicked() {
  dismissal_reason_ = password_manager::metrics_util::CLICKED_DONE;
}

void ManagePasswordsBubbleModel::OnManageLinkClicked() {
  dismissal_reason_ = password_manager::metrics_util::CLICKED_MANAGE;
  ManagePasswordsBubbleUIController::FromWebContents(web_contents_)
      ->NavigateToPasswordManagerSettingsPage();
}

void ManagePasswordsBubbleModel::OnPasswordAction(
    const autofill::PasswordForm& password_form,
    PasswordAction action) {
  if (!web_contents_)
    return;
  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  password_manager::PasswordStore* password_store =
      PasswordStoreFactory::GetForProfile(profile, Profile::EXPLICIT_ACCESS)
          .get();
  DCHECK(password_store);
  if (action == REMOVE_PASSWORD)
    password_store->RemoveLogin(password_form);
  else
    password_store->AddLogin(password_form);
}

void ManagePasswordsBubbleModel::WebContentsDestroyed(
    content::WebContents* web_contents) {
  // The WebContents have been destroyed.
  web_contents_ = NULL;
}
