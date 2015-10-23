// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/manage_passwords_bubble_model.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"
#include "chrome/browser/ui/passwords/manage_passwords_view_utils.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/feedback/feedback_data.h"
#include "components/feedback/feedback_util.h"
#include "components/password_manager/core/browser/password_bubble_experiment.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/common/credential_manager_types.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/password_manager/core/common/password_manager_ui.h"
#include "components/signin/core/browser/signin_manager.h"
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
  // TODO(vasilii): revive the function while implementing the smart bubble.
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

// A wrapper around password_bubble_experiment::IsSmartLockBrandingEnabled
// extracting the sync_service from the profile.
bool IsSmartLockBrandingEnabled(Profile* profile) {
  const ProfileSyncService* sync_service =
      ProfileSyncServiceFactory::GetForProfile(profile);
  return password_bubble_experiment::IsSmartLockBrandingEnabled(sync_service);
}

}  // namespace

ManagePasswordsBubbleModel::ManagePasswordsBubbleModel(
    content::WebContents* web_contents,
    DisplayReason display_reason)
    : content::WebContentsObserver(web_contents),
      display_disposition_(metrics_util::AUTOMATIC_WITH_PASSWORD_PENDING),
      dismissal_reason_(metrics_util::NO_DIRECT_INTERACTION),
      update_password_submission_event_(metrics_util::NO_UPDATE_SUBMISSION) {
  ManagePasswordsUIController* controller =
      ManagePasswordsUIController::FromWebContents(web_contents);

  origin_ = controller->origin();
  state_ = controller->state();
  password_overridden_ = controller->PasswordOverridden();
  if (state_ == password_manager::ui::PENDING_PASSWORD_STATE ||
      state_ == password_manager::ui::PENDING_PASSWORD_UPDATE_STATE) {
    pending_password_ = controller->PendingPassword();
    local_credentials_ = DeepCopyForms(controller->GetCurrentForms());
  } else if (state_ == password_manager::ui::CONFIRMATION_STATE) {
    // We don't need anything.
  } else if (state_ == password_manager::ui::CREDENTIAL_REQUEST_STATE) {
    local_credentials_ = DeepCopyForms(controller->GetCurrentForms());
    federated_credentials_ = DeepCopyForms(controller->GetFederatedForms());
  } else if (state_ == password_manager::ui::AUTO_SIGNIN_STATE) {
    pending_password_ = controller->PendingPassword();
  } else {
    local_credentials_ = DeepCopyForms(controller->GetCurrentForms());
  }

  if (state_ == password_manager::ui::PENDING_PASSWORD_STATE ||
      state_ == password_manager::ui::PENDING_PASSWORD_UPDATE_STATE) {
    UpdatePendingStateTitle();
  } else if (state_ == password_manager::ui::CONFIRMATION_STATE) {
    title_ =
        l10n_util::GetStringUTF16(IDS_MANAGE_PASSWORDS_CONFIRM_GENERATED_TITLE);
  } else if (state_ == password_manager::ui::CREDENTIAL_REQUEST_STATE) {
    title_ = l10n_util::GetStringUTF16(IDS_MANAGE_PASSWORDS_CHOOSE_TITLE);
  } else if (state_ == password_manager::ui::AUTO_SIGNIN_STATE) {
    // There is no title.
  } else if (state_ == password_manager::ui::MANAGE_STATE) {
    UpdateManageStateTitle();
  }

  if (state_ == password_manager::ui::CONFIRMATION_STATE) {
    base::string16 save_confirmation_link =
        l10n_util::GetStringUTF16(IDS_MANAGE_PASSWORDS_LINK);
    int confirmation_text_id = IDS_MANAGE_PASSWORDS_CONFIRM_GENERATED_TEXT;
    if (IsSmartLockBrandingEnabled(GetProfile())) {
      std::string management_hostname =
          GURL(chrome::kPasswordManagerAccountDashboardURL).host();
      save_confirmation_link = base::UTF8ToUTF16(management_hostname);
      confirmation_text_id =
          IDS_MANAGE_PASSWORDS_CONFIRM_GENERATED_SMART_LOCK_TEXT;
    }

    size_t offset;
    save_confirmation_text_ =
        l10n_util::GetStringFUTF16(
            confirmation_text_id, save_confirmation_link, &offset);
    save_confirmation_link_range_ =
        gfx::Range(offset, offset + save_confirmation_link.length());
  }

  manage_link_ =
      l10n_util::GetStringUTF16(IDS_OPTIONS_PASSWORDS_MANAGE_PASSWORDS_LINK);

  if (display_reason == USER_ACTION) {
    switch (state_) {
      case password_manager::ui::PENDING_PASSWORD_STATE:
        display_disposition_ = metrics_util::MANUAL_WITH_PASSWORD_PENDING;
        break;
      case password_manager::ui::PENDING_PASSWORD_UPDATE_STATE:
        display_disposition_ =
            metrics_util::MANUAL_WITH_PASSWORD_PENDING_UPDATE;
        break;
      case password_manager::ui::MANAGE_STATE:
        display_disposition_ = metrics_util::MANUAL_MANAGE_PASSWORDS;
        break;
      default:
        break;
    }
  } else {
    switch (state_) {
      case password_manager::ui::PENDING_PASSWORD_STATE:
        display_disposition_ = metrics_util::AUTOMATIC_WITH_PASSWORD_PENDING;
        break;
      case password_manager::ui::PENDING_PASSWORD_UPDATE_STATE:
        display_disposition_ =
            metrics_util::AUTOMATIC_WITH_PASSWORD_PENDING_UPDATE;
        break;
      case password_manager::ui::CONFIRMATION_STATE:
        display_disposition_ =
            metrics_util::AUTOMATIC_GENERATED_PASSWORD_CONFIRMATION;
        break;
      case password_manager::ui::CREDENTIAL_REQUEST_STATE:
        display_disposition_ = metrics_util::AUTOMATIC_CREDENTIAL_REQUEST;
        break;
      case password_manager::ui::AUTO_SIGNIN_STATE:
        display_disposition_ = metrics_util::AUTOMATIC_SIGNIN_TOAST;
        break;
      default:
        break;
    }
  }
  metrics_util::LogUIDisplayDisposition(display_disposition_);

  controller->OnBubbleShown();
}

ManagePasswordsBubbleModel::~ManagePasswordsBubbleModel() {
  if (state_ == password_manager::ui::PENDING_PASSWORD_STATE) {
    Profile* profile = GetProfile();
    if (profile && IsSmartLockBrandingEnabled(profile)) {
      profile->GetPrefs()->SetBoolean(
          password_manager::prefs::kWasSavePrompFirstRunExperienceShown, true);
    }
  }
  ManagePasswordsUIController* manage_passwords_ui_controller =
      web_contents() ?
          ManagePasswordsUIController::FromWebContents(web_contents())
          : nullptr;
  if (manage_passwords_ui_controller)
    manage_passwords_ui_controller->OnBubbleHidden();
  if (dismissal_reason_ == metrics_util::NOT_DISPLAYED)
    return;

  if (state_ != password_manager::ui::PENDING_PASSWORD_UPDATE_STATE) {
    // We have separate metrics for the Update bubble so do not record dismissal
    // reason for it.
    metrics_util::LogUIDismissalReason(dismissal_reason_);
  }
  // Other use cases have been reported in the callbacks like OnSaveClicked().
  if (state_ == password_manager::ui::PENDING_PASSWORD_STATE &&
      dismissal_reason_ == metrics_util::NO_DIRECT_INTERACTION)
    RecordExperimentStatistics(web_contents(), dismissal_reason_);
  // Check if this was update password and record update statistics.
  if (update_password_submission_event_ == metrics_util::NO_UPDATE_SUBMISSION &&
      (state_ == password_manager::ui::PENDING_PASSWORD_UPDATE_STATE ||
       state_ == password_manager::ui::PENDING_PASSWORD_STATE)) {
    update_password_submission_event_ =
        GetUpdateDismissalReason(NO_INTERACTION);
  }
  if (update_password_submission_event_ != metrics_util::NO_UPDATE_SUBMISSION)
    LogUpdatePasswordSubmissionEvent(update_password_submission_event_);
}
void ManagePasswordsBubbleModel::OnCancelClicked() {
  DCHECK_EQ(password_manager::ui::CREDENTIAL_REQUEST_STATE, state_);
  dismissal_reason_ = metrics_util::CLICKED_CANCEL;
}

void ManagePasswordsBubbleModel::OnNeverForThisSiteClicked() {
  dismissal_reason_ = metrics_util::CLICKED_NEVER;
  update_password_submission_event_ = GetUpdateDismissalReason(NOPE_CLICKED);
  RecordExperimentStatistics(web_contents(), dismissal_reason_);
  ManagePasswordsUIController* manage_passwords_ui_controller =
      ManagePasswordsUIController::FromWebContents(web_contents());
  manage_passwords_ui_controller->NeverSavePassword();
}

void ManagePasswordsBubbleModel::OnSaveClicked() {
  dismissal_reason_ = metrics_util::CLICKED_SAVE;
  RecordExperimentStatistics(web_contents(), dismissal_reason_);
  update_password_submission_event_ = GetUpdateDismissalReason(UPDATE_CLICKED);
  ManagePasswordsUIController* manage_passwords_ui_controller =
      ManagePasswordsUIController::FromWebContents(web_contents());
  manage_passwords_ui_controller->SavePassword();
}

void ManagePasswordsBubbleModel::OnNopeUpdateClicked() {
  update_password_submission_event_ = GetUpdateDismissalReason(NOPE_CLICKED);
}

void ManagePasswordsBubbleModel::OnUpdateClicked(
    const autofill::PasswordForm& password_form) {
  update_password_submission_event_ = GetUpdateDismissalReason(UPDATE_CLICKED);
  ManagePasswordsUIController* manage_passwords_ui_controller =
      ManagePasswordsUIController::FromWebContents(web_contents());
  manage_passwords_ui_controller->UpdatePassword(password_form);
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
  if (IsSmartLockBrandingEnabled(GetProfile())) {
    ManagePasswordsUIController::FromWebContents(web_contents())
        ->NavigateToExternalPasswordManager();
  } else {
    ManagePasswordsUIController::FromWebContents(web_contents())
        ->NavigateToPasswordManagerSettingsPage();
  }
}

void ManagePasswordsBubbleModel::OnBrandLinkClicked() {
  dismissal_reason_ = metrics_util::CLICKED_BRAND_NAME;
  ManagePasswordsUIController::FromWebContents(web_contents())
      ->NavigateToSmartLockPage();
}

void ManagePasswordsBubbleModel::OnAutoSignInToastTimeout() {
  dismissal_reason_ = metrics_util::AUTO_SIGNIN_TOAST_TIMEOUT;
}

void ManagePasswordsBubbleModel::OnAutoSignInClicked() {
  dismissal_reason_ = metrics_util::AUTO_SIGNIN_TOAST_CLICKED;
  ManagePasswordsUIController* manage_passwords_ui_controller =
      ManagePasswordsUIController::FromWebContents(web_contents());
  manage_passwords_ui_controller->ManageAccounts();
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
  ManagePasswordsUIController* manage_passwords_ui_controller =
      ManagePasswordsUIController::FromWebContents(web_contents());
  manage_passwords_ui_controller->ChooseCredential(password_form,
                                                   credential_type);
}

Profile* ManagePasswordsBubbleModel::GetProfile() const {
  return GetProfileFromWebContents(web_contents());
}

bool ManagePasswordsBubbleModel::ShouldShowMultipleAccountUpdateUI() const {
  ManagePasswordsUIController* controller =
      ManagePasswordsUIController::FromWebContents(web_contents());
  return controller->ShouldShowMultipleAccountUpdateUI();
}

bool ManagePasswordsBubbleModel::ShouldShowGoogleSmartLockWelcome() const {
  Profile* profile = GetProfile();
  if (IsSmartLockBrandingEnabled(profile)) {
    PrefService* prefs = profile->GetPrefs();
    return !prefs->GetBoolean(
        password_manager::prefs::kWasSavePrompFirstRunExperienceShown);
  }
  return false;
}

// static
int ManagePasswordsBubbleModel::UsernameFieldWidth() {
  return GetFieldWidth(USERNAME_FIELD);
}

// static
int ManagePasswordsBubbleModel::PasswordFieldWidth() {
  return GetFieldWidth(PASSWORD_FIELD);
}

void ManagePasswordsBubbleModel::UpdatePendingStateTitle() {
  title_brand_link_range_ = gfx::Range();
  GetSavePasswordDialogTitleTextAndLinkRange(
      web_contents()->GetVisibleURL(), origin(),
      IsSmartLockBrandingEnabled(GetProfile()),
      state_ == password_manager::ui::PENDING_PASSWORD_UPDATE_STATE, &title_,
      &title_brand_link_range_);
}

void ManagePasswordsBubbleModel::UpdateManageStateTitle() {
  GetManagePasswordsDialogTitleText(web_contents()->GetVisibleURL(), origin(),
                                    &title_);
}

metrics_util::UpdatePasswordSubmissionEvent
ManagePasswordsBubbleModel::GetUpdateDismissalReason(
    UserBehaviorOnUpdateBubble behavior) const {
  static const metrics_util::UpdatePasswordSubmissionEvent update_events[4][3] =
      {{metrics_util::NO_ACCOUNTS_CLICKED_UPDATE,
        metrics_util::NO_ACCOUNTS_CLICKED_NOPE,
        metrics_util::NO_ACCOUNTS_NO_INTERACTION},
       {metrics_util::ONE_ACCOUNT_CLICKED_UPDATE,
        metrics_util::ONE_ACCOUNT_CLICKED_NOPE,
        metrics_util::ONE_ACCOUNT_NO_INTERACTION},
       {metrics_util::MULTIPLE_ACCOUNTS_CLICKED_UPDATE,
        metrics_util::MULTIPLE_ACCOUNTS_CLICKED_NOPE,
        metrics_util::MULTIPLE_ACCOUNTS_NO_INTERACTION},
       {metrics_util::PASSWORD_OVERRIDDEN_CLICKED_UPDATE,
        metrics_util::PASSWORD_OVERRIDDEN_CLICKED_NOPE,
        metrics_util::PASSWORD_OVERRIDDEN_NO_INTERACTION}};

  if (state_ == password_manager::ui::PENDING_PASSWORD_STATE) {
    if (pending_password_.IsPossibleChangePasswordFormWithoutUsername())
      return update_events[0][behavior];
    return metrics_util::NO_UPDATE_SUBMISSION;
  }
  if (state_ != password_manager::ui::PENDING_PASSWORD_UPDATE_STATE)
    return metrics_util::NO_UPDATE_SUBMISSION;
  if (password_overridden_)
    return update_events[3][behavior];
  if (ShouldShowMultipleAccountUpdateUI())
    return update_events[2][behavior];
  return update_events[1][behavior];
}
