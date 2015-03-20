// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/manage_passwords_bubble_model.h"

#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"
#include "chrome/browser/ui/passwords/password_bubble_experiment.h"
#include "chrome/grit/generated_resources.h"
#include "components/feedback/feedback_data.h"
#include "components/feedback/feedback_util.h"
#include "components/password_manager/content/common/credential_manager_types.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/common/password_manager_ui.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

using autofill::PasswordFormMap;
using feedback::FeedbackData;
using content::WebContents;
namespace metrics_util = password_manager::metrics_util;

namespace {

enum FieldType { USERNAME_FIELD, PASSWORD_FIELD };

const int kUsernameFieldSize = 30;
const int kPasswordFieldSize = 22;

// Returns the width of |type| field.
int GetFieldWidth(FieldType type) {
  return ui::ResourceBundle::GetSharedInstance()
      .GetFontList(ui::ResourceBundle::SmallFont)
      .GetExpectedTextWidth(type == USERNAME_FIELD ? kUsernameFieldSize
                                                   : kPasswordFieldSize);
}

Profile* GetProfileFromWebContents(content::WebContents* web_contents) {
  if (!web_contents)
    return nullptr;
  return Profile::FromBrowserContext(web_contents->GetBrowserContext());
}

void RecordExperimentStatistics(content::WebContents* web_contents,
                                metrics_util::UIDismissalReason reason) {
  Profile* profile = GetProfileFromWebContents(web_contents);
  if (!profile)
    return;
  password_bubble_experiment::RecordBubbleClosed(profile->GetPrefs(), reason);
}

ScopedVector<const autofill::PasswordForm> DeepCopyForms(
    const std::vector<const autofill::PasswordForm*>& forms) {
  ScopedVector<const autofill::PasswordForm> result;
  result.reserve(forms.size());
  std::transform(forms.begin(), forms.end(), std::back_inserter(result),
                 [](const autofill::PasswordForm* form) {
    return new autofill::PasswordForm(*form);
  });
  return result.Pass();
}

}  // namespace

ManagePasswordsBubbleModel::ManagePasswordsBubbleModel(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      never_save_passwords_(false),
      display_disposition_(metrics_util::AUTOMATIC_WITH_PASSWORD_PENDING),
      dismissal_reason_(metrics_util::NOT_DISPLAYED) {
  ManagePasswordsUIController* controller =
      ManagePasswordsUIController::FromWebContents(web_contents);

  origin_ = controller->origin();
  state_ = controller->state();
  if (state_ == password_manager::ui::PENDING_PASSWORD_STATE) {
    pending_password_ = controller->PendingPassword();
    local_credentials_ = DeepCopyForms(controller->GetCurrentForms());
  } else if (state_ == password_manager::ui::CONFIRMATION_STATE) {
    // We don't need anything.
  } else if (state_ == password_manager::ui::CREDENTIAL_REQUEST_STATE) {
    local_credentials_ = DeepCopyForms(controller->GetCurrentForms());
    federated_credentials_ = DeepCopyForms(controller->GetFederatedForms());
  } else if (state_ == password_manager::ui::AUTO_SIGNIN_STATE) {
    pending_password_ = *controller->GetCurrentForms()[0];
  } else {
    local_credentials_ = DeepCopyForms(controller->GetCurrentForms());
  }

  if (state_ == password_manager::ui::PENDING_PASSWORD_STATE) {
    title_ = PendingStateTitleBasedOnSavePasswordPref();
  } else if (state_ == password_manager::ui::BLACKLIST_STATE) {
    title_ = l10n_util::GetStringUTF16(IDS_MANAGE_PASSWORDS_BLACKLISTED_TITLE);
  } else if (state_ == password_manager::ui::CONFIRMATION_STATE) {
    title_ =
        l10n_util::GetStringUTF16(IDS_MANAGE_PASSWORDS_CONFIRM_GENERATED_TITLE);
  } else if (state_ == password_manager::ui::CREDENTIAL_REQUEST_STATE) {
    title_ = l10n_util::GetStringUTF16(IDS_MANAGE_PASSWORDS_CHOOSE_TITLE);
  } else if (state_ == password_manager::ui::AUTO_SIGNIN_STATE) {
    // There is no title.
  } else {
    title_ = IsNewUIActive() ?
        l10n_util::GetStringFUTF16(IDS_MANAGE_ACCOUNTS_TITLE,
                                   base::UTF8ToUTF16(origin_.spec())) :
        l10n_util::GetStringUTF16(IDS_MANAGE_PASSWORDS_TITLE);
  }

  if (state_ == password_manager::ui::CONFIRMATION_STATE) {
    base::string16 save_confirmation_link =
        l10n_util::GetStringUTF16(IDS_MANAGE_PASSWORDS_CONFIRM_GENERATED_LINK);
    size_t offset;
    save_confirmation_text_ =
        l10n_util::GetStringFUTF16(IDS_MANAGE_PASSWORDS_CONFIRM_GENERATED_TEXT,
                                   save_confirmation_link, &offset);
    save_confirmation_link_range_ =
        gfx::Range(offset, offset + save_confirmation_link.length());
  }

  manage_link_ =
      l10n_util::GetStringUTF16(IDS_OPTIONS_PASSWORDS_MANAGE_PASSWORDS_LINK);
}

ManagePasswordsBubbleModel::~ManagePasswordsBubbleModel() {}

void ManagePasswordsBubbleModel::OnBubbleShown(
    ManagePasswordsBubble::DisplayReason reason) {
  if (reason == ManagePasswordsBubble::USER_ACTION) {
    if (state_ == password_manager::ui::PENDING_PASSWORD_STATE) {
      display_disposition_ = metrics_util::MANUAL_WITH_PASSWORD_PENDING;
    } else if (state_ == password_manager::ui::BLACKLIST_STATE) {
      display_disposition_ = metrics_util::MANUAL_BLACKLISTED;
    } else {
      display_disposition_ = metrics_util::MANUAL_MANAGE_PASSWORDS;
    }
  } else {
    if (state_ == password_manager::ui::CONFIRMATION_STATE) {
      display_disposition_ =
          metrics_util::AUTOMATIC_GENERATED_PASSWORD_CONFIRMATION;
    } else if (state_ == password_manager::ui::CREDENTIAL_REQUEST_STATE) {
      display_disposition_ = metrics_util::AUTOMATIC_CREDENTIAL_REQUEST;
    } else if (state_ == password_manager::ui::AUTO_SIGNIN_STATE) {
      display_disposition_ = metrics_util::AUTOMATIC_SIGNIN_TOAST;
    } else {
      display_disposition_ = metrics_util::AUTOMATIC_WITH_PASSWORD_PENDING;
    }
  }
  metrics_util::LogUIDisplayDisposition(display_disposition_);

  // Default to a dismissal reason of "no interaction". If the user interacts
  // with the button in such a way that it closes, we'll reset this value
  // accordingly.
  dismissal_reason_ = metrics_util::NO_DIRECT_INTERACTION;

  ManagePasswordsUIController* controller =
      ManagePasswordsUIController::FromWebContents(web_contents());
  controller->OnBubbleShown();
}

void ManagePasswordsBubbleModel::OnBubbleHidden() {
  ManagePasswordsUIController* manage_passwords_ui_controller =
      web_contents() ?
          ManagePasswordsUIController::FromWebContents(web_contents())
          : nullptr;
  if (manage_passwords_ui_controller)
    manage_passwords_ui_controller->OnBubbleHidden();
  if (dismissal_reason_ == metrics_util::NOT_DISPLAYED)
    return;

  metrics_util::LogUIDismissalReason(dismissal_reason_);
  // Other use cases have been reported in the callbacks like OnSaveClicked().
  if (state_ == password_manager::ui::PENDING_PASSWORD_STATE &&
      dismissal_reason_ == metrics_util::NO_DIRECT_INTERACTION)
    RecordExperimentStatistics(web_contents(), dismissal_reason_);
}

void ManagePasswordsBubbleModel::OnNopeClicked() {
  dismissal_reason_ = metrics_util::CLICKED_NOPE;
  RecordExperimentStatistics(web_contents(), dismissal_reason_);
  if (state_ != password_manager::ui::CREDENTIAL_REQUEST_STATE)
    state_ = password_manager::ui::PENDING_PASSWORD_STATE;
}

void ManagePasswordsBubbleModel::OnConfirmationForNeverForThisSite() {
  never_save_passwords_ = true;
  title_ = PendingStateTitleBasedOnSavePasswordPref();
}

void ManagePasswordsBubbleModel::OnUndoNeverForThisSite() {
  never_save_passwords_ = false;
  title_ = PendingStateTitleBasedOnSavePasswordPref();
}

void ManagePasswordsBubbleModel::OnNeverForThisSiteClicked() {
  dismissal_reason_ = metrics_util::CLICKED_NEVER;
  RecordExperimentStatistics(web_contents(), dismissal_reason_);
  ManagePasswordsUIController* manage_passwords_ui_controller =
      ManagePasswordsUIController::FromWebContents(web_contents());
  manage_passwords_ui_controller->NeverSavePassword();
  state_ = password_manager::ui::BLACKLIST_STATE;
}

void ManagePasswordsBubbleModel::OnUnblacklistClicked() {
  dismissal_reason_ = metrics_util::CLICKED_UNBLACKLIST;
  ManagePasswordsUIController* manage_passwords_ui_controller =
      ManagePasswordsUIController::FromWebContents(web_contents());
  manage_passwords_ui_controller->UnblacklistSite();
  state_ = password_manager::ui::MANAGE_STATE;
}

void ManagePasswordsBubbleModel::OnSaveClicked() {
  dismissal_reason_ = metrics_util::CLICKED_SAVE;
  RecordExperimentStatistics(web_contents(), dismissal_reason_);
  ManagePasswordsUIController* manage_passwords_ui_controller =
      ManagePasswordsUIController::FromWebContents(web_contents());
  manage_passwords_ui_controller->SavePassword();
  state_ = password_manager::ui::MANAGE_STATE;
}

void ManagePasswordsBubbleModel::OnDoneClicked() {
  dismissal_reason_ = metrics_util::CLICKED_DONE;
}

// TODO(gcasto): Is it worth having this be separate from OnDoneClicked()?
// User intent is pretty similar in both cases.
void ManagePasswordsBubbleModel::OnOKClicked() {
  dismissal_reason_ = metrics_util::CLICKED_OK;
}

void ManagePasswordsBubbleModel::OnManageLinkClicked() {
  dismissal_reason_ = metrics_util::CLICKED_MANAGE;
  ManagePasswordsUIController::FromWebContents(web_contents())
      ->NavigateToPasswordManagerSettingsPage();
}

void ManagePasswordsBubbleModel::OnAutoSignInToastTimeout() {
  dismissal_reason_ = metrics_util::AUTO_SIGNIN_TOAST_TIMEOUT;
}

void ManagePasswordsBubbleModel::OnAutoSignInClicked() {
  dismissal_reason_ = metrics_util::AUTO_SIGNIN_TOAST_CLICKED;
  ManagePasswordsUIController* manage_passwords_ui_controller =
      ManagePasswordsUIController::FromWebContents(web_contents());
  manage_passwords_ui_controller->ManageAccounts();
  state_ = password_manager::ui::MANAGE_STATE;
}

void ManagePasswordsBubbleModel::OnPasswordAction(
    const autofill::PasswordForm& password_form,
    PasswordAction action) {
  if (!web_contents())
    return;
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  password_manager::PasswordStore* password_store =
      PasswordStoreFactory::GetForProfile(
          profile, ServiceAccessType::EXPLICIT_ACCESS).get();
  DCHECK(password_store);
  if (action == REMOVE_PASSWORD)
    password_store->RemoveLogin(password_form);
  else
    password_store->AddLogin(password_form);
}

void ManagePasswordsBubbleModel::OnChooseCredentials(
    const autofill::PasswordForm& password_form,
    password_manager::CredentialType credential_type) {
  dismissal_reason_ = metrics_util::CLICKED_CREDENTIAL;
  RecordExperimentStatistics(web_contents(), dismissal_reason_);
  ManagePasswordsUIController* manage_passwords_ui_controller =
      ManagePasswordsUIController::FromWebContents(web_contents());
  manage_passwords_ui_controller->ChooseCredential(password_form,
                                                   credential_type);
  state_ = password_manager::ui::INACTIVE_STATE;
}

Profile* ManagePasswordsBubbleModel::GetProfile() const {
  return GetProfileFromWebContents(web_contents());
}

bool ManagePasswordsBubbleModel::IsNewUIActive() const {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableCredentialManagerAPI);
}

// static
int ManagePasswordsBubbleModel::UsernameFieldWidth() {
  return GetFieldWidth(USERNAME_FIELD);
}

// static
int ManagePasswordsBubbleModel::PasswordFieldWidth() {
  return GetFieldWidth(PASSWORD_FIELD);
}

base::string16
ManagePasswordsBubbleModel::PendingStateTitleBasedOnSavePasswordPref() const {
  int message_id = 0;
  if (never_save_passwords_)
    message_id = IDS_MANAGE_PASSWORDS_BLACKLIST_CONFIRMATION_TITLE;
  else if (IsNewUIActive())
    message_id = IDS_PASSWORD_MANAGER_SAVE_PASSWORD_SMART_LOCK_PROMPT;
  else
    message_id = IDS_SAVE_PASSWORD;
  return l10n_util::GetStringUTF16(message_id);
}
