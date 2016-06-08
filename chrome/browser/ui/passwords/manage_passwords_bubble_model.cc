// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/manage_passwords_bubble_model.h"

#include <stddef.h>

#include <algorithm>
#include <limits>
#include <string>
#include <vector>

#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/default_clock.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/passwords/manage_passwords_view_utils.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/browser_sync/browser/profile_sync_service.h"
#include "components/password_manager/core/browser/password_bubble_experiment.h"
#include "components/password_manager/core/browser/password_manager_constants.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/password_manager/core/common/password_manager_ui.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

namespace metrics_util = password_manager::metrics_util;

namespace {

Profile* GetProfileFromWebContents(content::WebContents* web_contents) {
  if (!web_contents)
    return nullptr;
  return Profile::FromBrowserContext(web_contents->GetBrowserContext());
}

void CleanStatisticsForSite(content::WebContents* web_contents,
                            const GURL& origin) {
  DCHECK(web_contents);
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  password_manager::PasswordStore* password_store =
      PasswordStoreFactory::GetForProfile(profile,
                                          ServiceAccessType::IMPLICIT_ACCESS)
          .get();
  password_store->RemoveSiteStats(origin.GetOrigin());
}

ScopedVector<const autofill::PasswordForm> DeepCopyForms(
    const std::vector<const autofill::PasswordForm*>& forms) {
  ScopedVector<const autofill::PasswordForm> result;
  result.reserve(forms.size());
  std::transform(forms.begin(), forms.end(), std::back_inserter(result),
                 [](const autofill::PasswordForm* form) {
    return new autofill::PasswordForm(*form);
  });
  return result;
}

password_bubble_experiment::SmartLockBranding GetSmartLockBrandingState(
    Profile* profile) {
  const ProfileSyncService* sync_service =
      ProfileSyncServiceFactory::GetForProfile(profile);
  return password_bubble_experiment::GetSmartLockBrandingState(sync_service);
}

}  // namespace

// Class responsible for collecting and reporting all the runtime interactions
// with the bubble.
class ManagePasswordsBubbleModel::InteractionKeeper {
 public:
  InteractionKeeper(
      password_manager::InteractionsStats stats,
      password_manager::metrics_util::UIDisplayDisposition display_disposition);

  ~InteractionKeeper() = default;

  // Records UMA events and updates the interaction statistics when the bubble
  // is closed.
  void ReportInteractions(const ManagePasswordsBubbleModel* model);

  void set_dismissal_reason(
      password_manager::metrics_util::UIDismissalReason reason) {
    dismissal_reason_ = reason;
  }

  void set_update_password_submission_event(
      password_manager::metrics_util::UpdatePasswordSubmissionEvent event) {
    update_password_submission_event_ = event;
  }

  void set_sign_in_promo_dismissal_reason(
      password_manager::metrics_util::SyncSignInUserAction reason) {
    sign_in_promo_dismissal_reason_ = reason;
  }

  void SetClockForTesting(std::unique_ptr<base::Clock> clock) {
    clock_ = std::move(clock);
  }

 private:
  // The way the bubble appeared.
  const password_manager::metrics_util::UIDisplayDisposition
  display_disposition_;

  // Dismissal reason for a bubble.
  password_manager::metrics_util::UIDismissalReason dismissal_reason_;

  // Dismissal reason for the update bubble.
  password_manager::metrics_util::UpdatePasswordSubmissionEvent
      update_password_submission_event_;

  // Dismissal reason for the Chrome Sign in bubble.
  password_manager::metrics_util::SyncSignInUserAction
      sign_in_promo_dismissal_reason_;

  // Current statistics for the save password bubble;
  password_manager::InteractionsStats interaction_stats_;

  // Used to retrieve the current time, in base::Time units.
  std::unique_ptr<base::Clock> clock_;

  DISALLOW_COPY_AND_ASSIGN(InteractionKeeper);
};

ManagePasswordsBubbleModel::InteractionKeeper::InteractionKeeper(
    password_manager::InteractionsStats stats,
    password_manager::metrics_util::UIDisplayDisposition display_disposition)
    : display_disposition_(display_disposition),
      dismissal_reason_(metrics_util::NO_DIRECT_INTERACTION),
      update_password_submission_event_(metrics_util::NO_UPDATE_SUBMISSION),
      sign_in_promo_dismissal_reason_(metrics_util::CHROME_SIGNIN_DISMISSED),
      interaction_stats_(std::move(stats)),
      clock_(new base::DefaultClock) {}

void ManagePasswordsBubbleModel::InteractionKeeper::ReportInteractions(
    const ManagePasswordsBubbleModel* model) {
  if (model->state() == password_manager::ui::PENDING_PASSWORD_STATE) {
    Profile* profile = model->GetProfile();
    if (profile) {
      if (GetSmartLockBrandingState(profile) ==
          password_bubble_experiment::SmartLockBranding::FULL) {
        password_bubble_experiment::RecordSavePromptFirstRunExperienceWasShown(
            profile->GetPrefs());
      }
      if (dismissal_reason_ == metrics_util::NO_DIRECT_INTERACTION &&
          display_disposition_ ==
              metrics_util::AUTOMATIC_WITH_PASSWORD_PENDING) {
        if (interaction_stats_.dismissal_count <
            std::numeric_limits<decltype(
                interaction_stats_.dismissal_count)>::max())
          interaction_stats_.dismissal_count++;
        interaction_stats_.update_time = clock_->Now();
        password_manager::PasswordStore* password_store =
            PasswordStoreFactory::GetForProfile(
                profile, ServiceAccessType::IMPLICIT_ACCESS).get();
        password_store->AddSiteStats(interaction_stats_);
      }
    }
  }

  if (model->state() == password_manager::ui::CHROME_SIGN_IN_PROMO_STATE) {
    metrics_util::LogAutoSigninPromoUserAction(sign_in_promo_dismissal_reason_);
    if (sign_in_promo_dismissal_reason_ ==
            password_manager::metrics_util::CHROME_SIGNIN_OK ||
        sign_in_promo_dismissal_reason_ ==
            password_manager::metrics_util::CHROME_SIGNIN_CANCEL) {
      DCHECK(model->web_contents());
      int show_count = model->GetProfile()->GetPrefs()->GetInteger(
          password_manager::prefs::kNumberSignInPasswordPromoShown);
      UMA_HISTOGRAM_COUNTS_100("PasswordManager.SignInPromoCountTilClick",
                               show_count);
    }
  } else if (model->state() !=
             password_manager::ui::PENDING_PASSWORD_UPDATE_STATE) {
    // We have separate metrics for the Update bubble so do not record dismissal
    // reason for it.
    metrics_util::LogUIDismissalReason(dismissal_reason_);
  }

  if (model->state() == password_manager::ui::PENDING_PASSWORD_UPDATE_STATE ||
      model->state() == password_manager::ui::PENDING_PASSWORD_STATE) {
    if (update_password_submission_event_ ==
        metrics_util::NO_UPDATE_SUBMISSION) {
      update_password_submission_event_ =
          model->GetUpdateDismissalReason(NO_INTERACTION);
      PasswordsModelDelegate* delegate =
          model->web_contents()
              ? PasswordsModelDelegateFromWebContents(model->web_contents())
              : nullptr;
      if (delegate &&
          model->state() == password_manager::ui::PENDING_PASSWORD_UPDATE_STATE)
        delegate->OnNoInteractionOnUpdate();
    }

    if (update_password_submission_event_ != metrics_util::NO_UPDATE_SUBMISSION)
      LogUpdatePasswordSubmissionEvent(update_password_submission_event_);
  }
}

ManagePasswordsBubbleModel::ManagePasswordsBubbleModel(
    content::WebContents* web_contents,
    DisplayReason display_reason)
    : content::WebContentsObserver(web_contents),
      password_overridden_(false) {
  PasswordsModelDelegate* delegate =
      PasswordsModelDelegateFromWebContents(web_contents);

  origin_ = delegate->GetOrigin();
  state_ = delegate->GetState();
  if (state_ == password_manager::ui::PENDING_PASSWORD_STATE ||
      state_ == password_manager::ui::PENDING_PASSWORD_UPDATE_STATE) {
    pending_password_ = delegate->GetPendingPassword();
    local_credentials_ = DeepCopyForms(delegate->GetCurrentForms());
  } else if (state_ == password_manager::ui::CONFIRMATION_STATE) {
    // We don't need anything.
  } else if (state_ == password_manager::ui::AUTO_SIGNIN_STATE) {
    pending_password_ = delegate->GetPendingPassword();
  } else {
    local_credentials_ = DeepCopyForms(delegate->GetCurrentForms());
  }

  if (state_ == password_manager::ui::PENDING_PASSWORD_STATE ||
      state_ == password_manager::ui::PENDING_PASSWORD_UPDATE_STATE) {
    UpdatePendingStateTitle();
  } else if (state_ == password_manager::ui::CONFIRMATION_STATE) {
    title_ =
        l10n_util::GetStringUTF16(IDS_MANAGE_PASSWORDS_CONFIRM_GENERATED_TITLE);
  } else if (state_ == password_manager::ui::AUTO_SIGNIN_STATE) {
    // There is no title.
  } else if (state_ == password_manager::ui::MANAGE_STATE) {
    UpdateManageStateTitle();
  }

  password_manager::InteractionsStats interaction_stats;
  if (state_ == password_manager::ui::CONFIRMATION_STATE) {
    base::string16 save_confirmation_link =
        l10n_util::GetStringUTF16(IDS_MANAGE_PASSWORDS_LINK);
    int confirmation_text_id = IDS_MANAGE_PASSWORDS_CONFIRM_GENERATED_TEXT;
    if (GetSmartLockBrandingState(GetProfile()) ==
        password_bubble_experiment::SmartLockBranding::FULL) {
      std::string management_hostname =
          GURL(password_manager::kPasswordManagerAccountDashboardURL).host();
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
  } else if (state_ == password_manager::ui::PENDING_PASSWORD_STATE) {
    interaction_stats.origin_domain = origin_.GetOrigin();
    interaction_stats.username_value = pending_password_.username_value;
    password_manager::InteractionsStats* stats =
        delegate->GetCurrentInteractionStats();
    if (stats) {
      DCHECK_EQ(interaction_stats.username_value, stats->username_value);
      DCHECK_EQ(interaction_stats.origin_domain, stats->origin_domain);
      interaction_stats.dismissal_count = stats->dismissal_count;
    }
  } else if (state_ == password_manager::ui::PENDING_PASSWORD_UPDATE_STATE) {
    password_overridden_ = delegate->IsPasswordOverridden();
  }

  manage_link_ =
      l10n_util::GetStringUTF16(IDS_OPTIONS_PASSWORDS_MANAGE_PASSWORDS_LINK);

  password_manager::metrics_util::UIDisplayDisposition display_disposition =
      metrics_util::AUTOMATIC_WITH_PASSWORD_PENDING;
  if (display_reason == USER_ACTION) {
    switch (state_) {
      case password_manager::ui::PENDING_PASSWORD_STATE:
        display_disposition = metrics_util::MANUAL_WITH_PASSWORD_PENDING;
        break;
      case password_manager::ui::PENDING_PASSWORD_UPDATE_STATE:
        display_disposition =
            metrics_util::MANUAL_WITH_PASSWORD_PENDING_UPDATE;
        break;
      case password_manager::ui::MANAGE_STATE:
        display_disposition = metrics_util::MANUAL_MANAGE_PASSWORDS;
        break;
      case password_manager::ui::CONFIRMATION_STATE:
      case password_manager::ui::CREDENTIAL_REQUEST_STATE:
      case password_manager::ui::AUTO_SIGNIN_STATE:
      case password_manager::ui::CHROME_SIGN_IN_PROMO_STATE:
      case password_manager::ui::INACTIVE_STATE:
        NOTREACHED();
        break;
    }
  } else {
    switch (state_) {
      case password_manager::ui::PENDING_PASSWORD_STATE:
        display_disposition = metrics_util::AUTOMATIC_WITH_PASSWORD_PENDING;
        break;
      case password_manager::ui::PENDING_PASSWORD_UPDATE_STATE:
        display_disposition =
            metrics_util::AUTOMATIC_WITH_PASSWORD_PENDING_UPDATE;
        break;
      case password_manager::ui::CONFIRMATION_STATE:
        display_disposition =
            metrics_util::AUTOMATIC_GENERATED_PASSWORD_CONFIRMATION;
        break;
      case password_manager::ui::AUTO_SIGNIN_STATE:
        display_disposition = metrics_util::AUTOMATIC_SIGNIN_TOAST;
        break;
      case password_manager::ui::MANAGE_STATE:
      case password_manager::ui::CREDENTIAL_REQUEST_STATE:
      case password_manager::ui::CHROME_SIGN_IN_PROMO_STATE:
      case password_manager::ui::INACTIVE_STATE:
        NOTREACHED();
        break;
    }
  }
  metrics_util::LogUIDisplayDisposition(display_disposition);
  interaction_keeper_.reset(new InteractionKeeper(std::move(interaction_stats),
                                                  display_disposition));

  delegate->OnBubbleShown();
}

ManagePasswordsBubbleModel::~ManagePasswordsBubbleModel() {
  interaction_keeper_->ReportInteractions(this);
  // web_contents() is nullptr if the tab is closing.
  PasswordsModelDelegate* delegate =
      web_contents() ? PasswordsModelDelegateFromWebContents(web_contents())
                     : nullptr;
  if (delegate)
    delegate->OnBubbleHidden();
}

void ManagePasswordsBubbleModel::OnNeverForThisSiteClicked() {
  DCHECK_EQ(password_manager::ui::PENDING_PASSWORD_STATE, state_);
  interaction_keeper_->set_dismissal_reason(metrics_util::CLICKED_NEVER);
  interaction_keeper_->set_update_password_submission_event(
      GetUpdateDismissalReason(NOPE_CLICKED));
  CleanStatisticsForSite(web_contents(), origin_);
  PasswordsModelDelegateFromWebContents(web_contents())->NeverSavePassword();
}

void ManagePasswordsBubbleModel::OnSaveClicked() {
  DCHECK_EQ(password_manager::ui::PENDING_PASSWORD_STATE, state_);
  interaction_keeper_->set_dismissal_reason(metrics_util::CLICKED_SAVE);
  interaction_keeper_->set_update_password_submission_event(
      GetUpdateDismissalReason(UPDATE_CLICKED));
  CleanStatisticsForSite(web_contents(), origin_);
  PasswordsModelDelegateFromWebContents(web_contents())->SavePassword();
}

void ManagePasswordsBubbleModel::OnNopeUpdateClicked() {
  interaction_keeper_->set_update_password_submission_event(
      GetUpdateDismissalReason(NOPE_CLICKED));
  PasswordsModelDelegateFromWebContents(web_contents())->OnNopeUpdateClicked();
}

void ManagePasswordsBubbleModel::OnUpdateClicked(
    const autofill::PasswordForm& password_form) {
  interaction_keeper_->set_update_password_submission_event(
      GetUpdateDismissalReason(UPDATE_CLICKED));
  PasswordsModelDelegateFromWebContents(web_contents())->UpdatePassword(
      password_form);
}

void ManagePasswordsBubbleModel::OnDoneClicked() {
  interaction_keeper_->set_dismissal_reason(metrics_util::CLICKED_DONE);
}

// TODO(gcasto): Is it worth having this be separate from OnDoneClicked()?
// User intent is pretty similar in both cases.
void ManagePasswordsBubbleModel::OnOKClicked() {
  interaction_keeper_->set_dismissal_reason(metrics_util::CLICKED_OK);
}

void ManagePasswordsBubbleModel::OnManageLinkClicked() {
  interaction_keeper_->set_dismissal_reason(metrics_util::CLICKED_MANAGE);
  if (GetSmartLockBrandingState(GetProfile()) ==
      password_bubble_experiment::SmartLockBranding::FULL) {
    PasswordsModelDelegateFromWebContents(web_contents())
        ->NavigateToExternalPasswordManager();
  } else {
    PasswordsModelDelegateFromWebContents(web_contents())
        ->NavigateToPasswordManagerSettingsPage();
  }
}

void ManagePasswordsBubbleModel::OnBrandLinkClicked() {
  interaction_keeper_->set_dismissal_reason(metrics_util::CLICKED_BRAND_NAME);
  PasswordsModelDelegateFromWebContents(web_contents())
      ->NavigateToSmartLockHelpPage();
}

void ManagePasswordsBubbleModel::OnAutoSignInToastTimeout() {
  interaction_keeper_->set_dismissal_reason(
      metrics_util::AUTO_SIGNIN_TOAST_TIMEOUT);
}

void ManagePasswordsBubbleModel::OnPasswordAction(
    const autofill::PasswordForm& password_form,
    PasswordAction action) {
  Profile* profile = GetProfile();
  if (!profile)
    return;
  password_manager::PasswordStore* password_store =
      PasswordStoreFactory::GetForProfile(
          profile, ServiceAccessType::EXPLICIT_ACCESS).get();
  DCHECK(password_store);
  if (action == REMOVE_PASSWORD)
    password_store->RemoveLogin(password_form);
  else
    password_store->AddLogin(password_form);
}

void ManagePasswordsBubbleModel::OnSignInToChromeClicked() {
  interaction_keeper_->set_sign_in_promo_dismissal_reason(
      metrics_util::CHROME_SIGNIN_OK);
  GetProfile()->GetPrefs()->SetBoolean(
      password_manager::prefs::kWasSignInPasswordPromoClicked, true);
  PasswordsModelDelegateFromWebContents(web_contents())
      ->NavigateToChromeSignIn();
}

void ManagePasswordsBubbleModel::OnSkipSignInClicked() {
  interaction_keeper_->set_sign_in_promo_dismissal_reason(
      metrics_util::CHROME_SIGNIN_CANCEL);
  GetProfile()->GetPrefs()->SetBoolean(
      password_manager::prefs::kWasSignInPasswordPromoClicked, true);
}

Profile* ManagePasswordsBubbleModel::GetProfile() const {
  return GetProfileFromWebContents(web_contents());
}

bool ManagePasswordsBubbleModel::ShouldShowMultipleAccountUpdateUI() const {
  return state_ == password_manager::ui::PENDING_PASSWORD_UPDATE_STATE &&
         local_credentials_.size() > 1 && !password_overridden_;
}

bool ManagePasswordsBubbleModel::ShouldShowGoogleSmartLockWelcome() const {
  Profile* profile = GetProfile();
  if (GetSmartLockBrandingState(profile) ==
      password_bubble_experiment::SmartLockBranding::FULL) {
    PrefService* prefs = profile->GetPrefs();
    return !prefs->GetBoolean(
        password_manager::prefs::kWasSavePrompFirstRunExperienceShown);
  }
  return false;
}

bool ManagePasswordsBubbleModel::ReplaceToShowSignInPromoIfNeeded() {
  DCHECK_EQ(password_manager::ui::PENDING_PASSWORD_STATE, state_);
  PrefService* prefs = GetProfile()->GetPrefs();
  const ProfileSyncService* sync_service =
      ProfileSyncServiceFactory::GetForProfile(GetProfile());
  if (password_bubble_experiment::ShouldShowChromeSignInPasswordPromo(
          prefs, sync_service)) {
    interaction_keeper_->ReportInteractions(this);
    title_brand_link_range_ = gfx::Range();
    title_ = l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_SIGNIN_PROMO_TITLE);
    state_ = password_manager::ui::CHROME_SIGN_IN_PROMO_STATE;
    int show_count = prefs->GetInteger(
        password_manager::prefs::kNumberSignInPasswordPromoShown);
    prefs->SetInteger(password_manager::prefs::kNumberSignInPasswordPromoShown,
                      show_count + 1);
    return true;
  }
  return false;
}

void ManagePasswordsBubbleModel::SetClockForTesting(
    std::unique_ptr<base::Clock> clock) {
  interaction_keeper_->SetClockForTesting(std::move(clock));
}

void ManagePasswordsBubbleModel::UpdatePendingStateTitle() {
  title_brand_link_range_ = gfx::Range();
  PasswordTittleType type =
      state_ == password_manager::ui::PENDING_PASSWORD_UPDATE_STATE
          ? PasswordTittleType::UPDATE_PASSWORD
          : (pending_password_.federation_origin.unique()
                 ? PasswordTittleType::SAVE_PASSWORD
                 : PasswordTittleType::SAVE_ACCOUNT);
  GetSavePasswordDialogTitleTextAndLinkRange(
      web_contents()->GetVisibleURL(), origin_,
      GetSmartLockBrandingState(GetProfile()) !=
          password_bubble_experiment::SmartLockBranding::NONE,
      type, &title_, &title_brand_link_range_);
}

void ManagePasswordsBubbleModel::UpdateManageStateTitle() {
  GetManagePasswordsDialogTitleText(web_contents()->GetVisibleURL(), origin_,
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
