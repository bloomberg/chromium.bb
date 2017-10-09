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
#include "chrome/grit/generated_resources.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/password_manager/core/browser/password_bubble_experiment.h"
#include "components/password_manager/core/browser/password_form_metrics_recorder.h"
#include "components/password_manager/core/browser/password_manager_constants.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/password_manager/core/common/password_manager_ui.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_WIN)
#include "chrome/browser/ui/desktop_ios_promotion/desktop_ios_promotion_util.h"
#endif

namespace metrics_util = password_manager::metrics_util;

namespace {

Profile* GetProfileFromWebContents(content::WebContents* web_contents) {
  if (!web_contents)
    return nullptr;
  return Profile::FromBrowserContext(web_contents->GetBrowserContext());
}

void CleanStatisticsForSite(Profile* profile, const GURL& origin) {
  DCHECK(profile);
  password_manager::PasswordStore* password_store =
      PasswordStoreFactory::GetForProfile(profile,
                                          ServiceAccessType::IMPLICIT_ACCESS)
          .get();
  password_store->RemoveSiteStats(origin.GetOrigin());
}

std::vector<autofill::PasswordForm> DeepCopyForms(
    const std::vector<std::unique_ptr<autofill::PasswordForm>>& forms) {
  std::vector<autofill::PasswordForm> result;
  result.reserve(forms.size());
  std::transform(forms.begin(), forms.end(), std::back_inserter(result),
                 [](const std::unique_ptr<autofill::PasswordForm>& form) {
                   return *form;
                 });
  return result;
}

bool IsSmartLockUser(Profile* profile) {
  const browser_sync::ProfileSyncService* sync_service =
      ProfileSyncServiceFactory::GetForProfile(profile);
  return password_bubble_experiment::IsSmartLockUser(sync_service);
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

  // Records UMA events, updates the interaction statistics and sends
  // notifications to the delegate when the bubble is closed.
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

  void set_sign_in_promo_shown_count(int count) {
    sign_in_promo_shown_count = count;
  }

 private:
  static password_manager::metrics_util::UIDismissalReason
  ToNearestUIDismissalReason(
      password_manager::metrics_util::UpdatePasswordSubmissionEvent
          update_password_submission_event);

  // The way the bubble appeared.
  const password_manager::metrics_util::UIDisplayDisposition
  display_disposition_;

  // Dismissal reason for a save bubble. Not filled for update bubbles.
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

  // Number of times the sign-in promo was shown to the user.
  int sign_in_promo_shown_count;

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
      clock_(new base::DefaultClock),
      sign_in_promo_shown_count(0) {}

void ManagePasswordsBubbleModel::InteractionKeeper::ReportInteractions(
    const ManagePasswordsBubbleModel* model) {
  if (model->state() == password_manager::ui::PENDING_PASSWORD_STATE) {
    Profile* profile = model->GetProfile();
    if (profile) {
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
    metrics_util::LogSyncSigninPromoUserAction(sign_in_promo_dismissal_reason_);
    switch (sign_in_promo_dismissal_reason_) {
      case password_manager::metrics_util::CHROME_SIGNIN_OK:
        UMA_HISTOGRAM_COUNTS_100("PasswordManager.SignInPromoCountTilSignIn",
                                 sign_in_promo_shown_count);
        break;
      case password_manager::metrics_util::CHROME_SIGNIN_CANCEL:
        UMA_HISTOGRAM_COUNTS_100("PasswordManager.SignInPromoCountTilNoThanks",
                                 sign_in_promo_shown_count);
        break;
      case password_manager::metrics_util::CHROME_SIGNIN_DISMISSED:
        UMA_HISTOGRAM_COUNTS_100("PasswordManager.SignInPromoDismissalCount",
                                 sign_in_promo_shown_count);
        break;
      case password_manager::metrics_util::CHROME_SIGNIN_ACTION_COUNT:
        NOTREACHED();
        break;
    }
  } else if (model->state() !=
             password_manager::ui::PENDING_PASSWORD_UPDATE_STATE) {
    // We have separate metrics for the Update bubble so do not record dismissal
    // reason for it.
    metrics_util::LogUIDismissalReason(dismissal_reason_);
  }

  if (model->state() == password_manager::ui::PENDING_PASSWORD_UPDATE_STATE ||
      model->state() == password_manager::ui::PENDING_PASSWORD_STATE) {
    // Send a notification if there was no interaction with the bubble.
    bool no_interaction =
        model->state() == password_manager::ui::PENDING_PASSWORD_UPDATE_STATE
            ? update_password_submission_event_ ==
                  metrics_util::NO_UPDATE_SUBMISSION
            : dismissal_reason_ == metrics_util::NO_DIRECT_INTERACTION;
    if (no_interaction && model->delegate_) {
      model->delegate_->OnNoInteraction();
    }

    // Send UMA.
    if (update_password_submission_event_ ==
        metrics_util::NO_UPDATE_SUBMISSION) {
      update_password_submission_event_ =
          model->GetUpdateDismissalReason(NO_INTERACTION);
    }
    if (update_password_submission_event_ != metrics_util::NO_UPDATE_SUBMISSION)
      LogUpdatePasswordSubmissionEvent(update_password_submission_event_);
  }

  // Record UKM statistics on dismissal reason.
  if (model->delegate_ && model->delegate_->GetPasswordFormMetricsRecorder()) {
    if (model->state() != password_manager::ui::PENDING_PASSWORD_UPDATE_STATE) {
      model->delegate_->GetPasswordFormMetricsRecorder()
          ->RecordUIDismissalReason(dismissal_reason_);
    } else {
      model->delegate_->GetPasswordFormMetricsRecorder()
          ->RecordUIDismissalReason(
              ToNearestUIDismissalReason(update_password_submission_event_));
    }
  }
}

// static
password_manager::metrics_util::UIDismissalReason
ManagePasswordsBubbleModel::InteractionKeeper::ToNearestUIDismissalReason(
    password_manager::metrics_util::UpdatePasswordSubmissionEvent
        update_password_submission_event) {
  switch (update_password_submission_event) {
    case password_manager::metrics_util::NO_ACCOUNTS_CLICKED_UPDATE:
    case password_manager::metrics_util::ONE_ACCOUNT_CLICKED_UPDATE:
    case password_manager::metrics_util::MULTIPLE_ACCOUNTS_CLICKED_UPDATE:
    case password_manager::metrics_util::PASSWORD_OVERRIDDEN_CLICKED_UPDATE:
      return password_manager::metrics_util::CLICKED_SAVE;

    case password_manager::metrics_util::NO_ACCOUNTS_CLICKED_NOPE:
    case password_manager::metrics_util::ONE_ACCOUNT_CLICKED_NOPE:
    case password_manager::metrics_util::MULTIPLE_ACCOUNTS_CLICKED_NOPE:
    case password_manager::metrics_util::PASSWORD_OVERRIDDEN_CLICKED_NOPE:
      return password_manager::metrics_util::CLICKED_CANCEL;

    case password_manager::metrics_util::NO_ACCOUNTS_NO_INTERACTION:
    case password_manager::metrics_util::ONE_ACCOUNT_NO_INTERACTION:
    case password_manager::metrics_util::MULTIPLE_ACCOUNTS_NO_INTERACTION:
    case password_manager::metrics_util::PASSWORD_OVERRIDDEN_NO_INTERACTION:
    case password_manager::metrics_util::NO_UPDATE_SUBMISSION:
      return password_manager::metrics_util::NO_DIRECT_INTERACTION;

    case password_manager::metrics_util::UPDATE_PASSWORD_EVENT_COUNT:
      // Not reached.
      break;
  }
  NOTREACHED();
  return password_manager::metrics_util::NO_DIRECT_INTERACTION;
}

ManagePasswordsBubbleModel::ManagePasswordsBubbleModel(
    base::WeakPtr<PasswordsModelDelegate> delegate,
    DisplayReason display_reason)
    : password_overridden_(false),
      delegate_(std::move(delegate)) {
  origin_ = delegate_->GetOrigin();
  state_ = delegate_->GetState();
  password_manager::InteractionsStats interaction_stats;
  if (state_ == password_manager::ui::PENDING_PASSWORD_STATE ||
      state_ == password_manager::ui::PENDING_PASSWORD_UPDATE_STATE) {
    pending_password_ = delegate_->GetPendingPassword();
    if (state_ == password_manager::ui::PENDING_PASSWORD_UPDATE_STATE) {
      local_credentials_ = DeepCopyForms(delegate_->GetCurrentForms());
      password_overridden_ = delegate_->IsPasswordOverridden();
    } else {
      interaction_stats.origin_domain = origin_.GetOrigin();
      interaction_stats.username_value = pending_password_.username_value;
      const password_manager::InteractionsStats* stats =
          delegate_->GetCurrentInteractionStats();
      if (stats) {
        DCHECK_EQ(interaction_stats.username_value, stats->username_value);
        DCHECK_EQ(interaction_stats.origin_domain, stats->origin_domain);
        interaction_stats.dismissal_count = stats->dismissal_count;
      }
    }
    hide_eye_icon_ = delegate_->BubbleIsManualFallbackForSaving()
                         ? pending_password_.form_has_autofilled_value
                         : display_reason == USER_ACTION;
    UpdatePendingStateTitle();
  } else if (state_ == password_manager::ui::CONFIRMATION_STATE) {
    title_ =
        l10n_util::GetStringUTF16(IDS_MANAGE_PASSWORDS_CONFIRM_GENERATED_TITLE);
  } else if (state_ == password_manager::ui::AUTO_SIGNIN_STATE) {
    pending_password_ = delegate_->GetPendingPassword();
  } else if (state_ == password_manager::ui::MANAGE_STATE) {
    local_credentials_ = DeepCopyForms(delegate_->GetCurrentForms());
    UpdateManageStateTitle();
    manage_link_ = l10n_util::GetStringUTF16(IDS_MANAGE_PASSWORDS_BUBBLE_LINK);
  }

  if (state_ == password_manager::ui::CONFIRMATION_STATE) {
    base::string16 save_confirmation_link = base::UTF8ToUTF16(
        GURL(base::ASCIIToUTF16(
                 password_manager::kPasswordManagerAccountDashboardURL))
            .host());

    size_t offset;
    save_confirmation_text_ =
        l10n_util::GetStringFUTF16(IDS_MANAGE_PASSWORDS_CONFIRM_GENERATED_TEXT,
                                   save_confirmation_link, &offset);
    save_confirmation_link_range_ =
        gfx::Range(offset, offset + save_confirmation_link.length());
  }

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
      case password_manager::ui::CHROME_DESKTOP_IOS_PROMO_STATE:
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
      case password_manager::ui::CHROME_DESKTOP_IOS_PROMO_STATE:
      case password_manager::ui::INACTIVE_STATE:
        NOTREACHED();
        break;
    }
  }

  if (delegate_ && delegate_->GetPasswordFormMetricsRecorder()) {
    delegate_->GetPasswordFormMetricsRecorder()->RecordPasswordBubbleShown(
        delegate_->GetCredentialSource(), display_disposition);
  }
  metrics_util::LogUIDisplayDisposition(display_disposition);
  interaction_keeper_.reset(new InteractionKeeper(std::move(interaction_stats),
                                                  display_disposition));

  delegate_->OnBubbleShown();
}

ManagePasswordsBubbleModel::~ManagePasswordsBubbleModel() {
  interaction_keeper_->ReportInteractions(this);
  if (delegate_)
    delegate_->OnBubbleHidden();
}

void ManagePasswordsBubbleModel::OnNeverForThisSiteClicked() {
  DCHECK_EQ(password_manager::ui::PENDING_PASSWORD_STATE, state_);
  interaction_keeper_->set_dismissal_reason(metrics_util::CLICKED_NEVER);
  interaction_keeper_->set_update_password_submission_event(
      GetUpdateDismissalReason(NOPE_CLICKED));
  CleanStatisticsForSite(GetProfile(), origin_);
  delegate_->NeverSavePassword();
}

void ManagePasswordsBubbleModel::OnCredentialEdited(
    base::string16 new_username,
    base::string16 new_password) {
  DCHECK_EQ(password_manager::ui::PENDING_PASSWORD_STATE, state_);
  pending_password_.username_value = std::move(new_username);
  pending_password_.password_value = std::move(new_password);
}

void ManagePasswordsBubbleModel::OnSaveClicked() {
  DCHECK_EQ(password_manager::ui::PENDING_PASSWORD_STATE, state_);
  interaction_keeper_->set_dismissal_reason(metrics_util::CLICKED_SAVE);
  interaction_keeper_->set_update_password_submission_event(
      GetUpdateDismissalReason(UPDATE_CLICKED));
  CleanStatisticsForSite(GetProfile(), origin_);
  delegate_->SavePassword(pending_password_.username_value,
                          pending_password_.password_value);
}

void ManagePasswordsBubbleModel::OnNopeUpdateClicked() {
  interaction_keeper_->set_update_password_submission_event(
      GetUpdateDismissalReason(NOPE_CLICKED));
  delegate_->OnNopeUpdateClicked();
}

void ManagePasswordsBubbleModel::OnUpdateClicked(
    const autofill::PasswordForm& password_form) {
  interaction_keeper_->set_update_password_submission_event(
      GetUpdateDismissalReason(UPDATE_CLICKED));
  delegate_->UpdatePassword(password_form);
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
  delegate_->NavigateToPasswordManagerSettingsPage();
}

void ManagePasswordsBubbleModel::
    OnNavigateToPasswordManagerAccountDashboardLinkClicked() {
  interaction_keeper_->set_dismissal_reason(
      metrics_util::CLICKED_PASSWORDS_DASHBOARD);
  delegate_->NavigateToPasswordManagerAccountDashboard();
}

void ManagePasswordsBubbleModel::OnBrandLinkClicked() {
  interaction_keeper_->set_dismissal_reason(metrics_util::CLICKED_BRAND_NAME);
  delegate_->NavigateToSmartLockHelpPage();
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
  delegate_->NavigateToChromeSignIn();
}

void ManagePasswordsBubbleModel::OnSkipSignInClicked() {
  interaction_keeper_->set_sign_in_promo_dismissal_reason(
      metrics_util::CHROME_SIGNIN_CANCEL);
  GetProfile()->GetPrefs()->SetBoolean(
      password_manager::prefs::kWasSignInPasswordPromoClicked, true);
}

Profile* ManagePasswordsBubbleModel::GetProfile() const {
  return GetProfileFromWebContents(GetWebContents());
}

content::WebContents* ManagePasswordsBubbleModel::GetWebContents() const {
  return delegate_ ? delegate_->GetWebContents() : nullptr;
}

bool ManagePasswordsBubbleModel::ShouldShowMultipleAccountUpdateUI() const {
  return state_ == password_manager::ui::PENDING_PASSWORD_UPDATE_STATE &&
         local_credentials_.size() > 1 && !password_overridden_;
}

bool ManagePasswordsBubbleModel::ReplaceToShowPromotionIfNeeded() {
  DCHECK_EQ(password_manager::ui::PENDING_PASSWORD_STATE, state_);
  PrefService* prefs = GetProfile()->GetPrefs();
  const browser_sync::ProfileSyncService* sync_service =
      ProfileSyncServiceFactory::GetForProfile(GetProfile());
  // Signin promotion.
  if (password_bubble_experiment::ShouldShowChromeSignInPasswordPromo(
          prefs, sync_service)) {
    interaction_keeper_->ReportInteractions(this);
    title_brand_link_range_ = gfx::Range();
    title_ = l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_SIGNIN_PROMO_TITLE);
    state_ = password_manager::ui::CHROME_SIGN_IN_PROMO_STATE;
    int show_count = prefs->GetInteger(
        password_manager::prefs::kNumberSignInPasswordPromoShown);
    show_count++;
    prefs->SetInteger(password_manager::prefs::kNumberSignInPasswordPromoShown,
                      show_count);
    interaction_keeper_->set_sign_in_promo_shown_count(show_count);
    return true;
  }
#if defined(OS_WIN)
  // Desktop to mobile promotion only enabled on windows.
  if (desktop_ios_promotion::IsEligibleForIOSPromotion(
          GetProfile(),
          desktop_ios_promotion::PromotionEntryPoint::SAVE_PASSWORD_BUBBLE)) {
    interaction_keeper_->ReportInteractions(this);
    title_brand_link_range_ = gfx::Range();
    title_ = desktop_ios_promotion::GetPromoTitle(
        desktop_ios_promotion::PromotionEntryPoint::SAVE_PASSWORD_BUBBLE);
    state_ = password_manager::ui::CHROME_DESKTOP_IOS_PROMO_STATE;
    return true;
  }
#endif
  return false;
}

void ManagePasswordsBubbleModel::SetClockForTesting(
    std::unique_ptr<base::Clock> clock) {
  interaction_keeper_->SetClockForTesting(std::move(clock));
}

void ManagePasswordsBubbleModel::UpdatePendingStateTitle() {
  title_brand_link_range_ = gfx::Range();
  PasswordTitleType type =
      state_ == password_manager::ui::PENDING_PASSWORD_UPDATE_STATE
          ? PasswordTitleType::UPDATE_PASSWORD
          : (pending_password_.federation_origin.unique()
                 ? PasswordTitleType::SAVE_PASSWORD
                 : PasswordTitleType::SAVE_ACCOUNT);
  GetSavePasswordDialogTitleTextAndLinkRange(
      GetWebContents()->GetVisibleURL(), origin_, IsSmartLockUser(GetProfile()),
      type, &title_, &title_brand_link_range_);
}

void ManagePasswordsBubbleModel::UpdateManageStateTitle() {
  GetManagePasswordsDialogTitleText(GetWebContents()->GetVisibleURL(), origin_,
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
