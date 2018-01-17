// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/dice_turn_sync_on_helper.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service_factory.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/signin/account_tracker_service_factory.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/startup/startup_types.h"
#include "chrome/browser/ui/tab_dialogs.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/browser/ui/webui/signin/signin_utils_desktop.h"
#include "chrome/common/url_constants.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/account_info.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/profile_management_switches.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_metrics.h"
#include "components/signin/core/browser/signin_pref_names.h"
#include "components/sync/base/sync_prefs.h"
#include "net/url_request/url_request_context_getter.h"

namespace {

// UMA histogram for tracking what users do when presented with the signin
// screen.
// Hence,
//   (a) existing enumerated constants should never be deleted or reordered, and
//   (b) new constants should only be appended at the end of the enumeration.
//
// Keep this in sync with SigninChoice in histograms.xml.
enum SigninChoice {
  SIGNIN_CHOICE_CANCEL = 0,
  SIGNIN_CHOICE_CONTINUE = 1,
  SIGNIN_CHOICE_NEW_PROFILE = 2,
  // SIGNIN_CHOICE_SIZE should always be last - this is a count of the number
  // of items in this enum.
  SIGNIN_CHOICE_SIZE,
};

void SetUserChoiceHistogram(SigninChoice choice) {
  UMA_HISTOGRAM_ENUMERATION("Enterprise.UserSigninChoice", choice,
                            SIGNIN_CHOICE_SIZE);
}

AccountInfo GetAccountInfo(Profile* profile, const std::string& account_id) {
  return AccountTrackerServiceFactory::GetForProfile(profile)->GetAccountInfo(
      account_id);
}

// If the |browser| argument is non-null, returns the pointer directly.
// Otherwise creates a new browser for the given profile on the given desktop,
// adds an empty tab and makes sure the browser is visible.
Browser* EnsureBrowser(Browser* browser, Profile* profile) {
  if (!browser) {
    // The user just created a new profile or has closed the browser that
    // we used previously. Grab the most recently active browser or else
    // create a new one.
    browser = chrome::FindLastActiveWithProfile(profile);
    if (!browser) {
      browser = new Browser(Browser::CreateParams(profile, true));
      chrome::AddTabAt(browser, GURL(), -1, true);
    }
    browser->window()->Show();
  }
  return browser;
}

void StartNewSigninInNewProfile(Profile* new_profile,
                                const std::string& username) {
  profiles::FindOrCreateNewWindowForProfile(
      new_profile, chrome::startup::IS_PROCESS_STARTUP,
      chrome::startup::IS_FIRST_RUN, false);
  Browser* browser = chrome::FindTabbedBrowser(new_profile, false);
  browser->signin_view_controller()->ShowDiceSigninTab(
      profiles::BUBBLE_VIEW_MODE_GAIA_SIGNIN, browser,
      signin_metrics::AccessPoint::ACCESS_POINT_START_PAGE, username);
}

}  // namespace

DiceTurnSyncOnHelper::SigninDialogDelegate::SigninDialogDelegate(
    base::WeakPtr<DiceTurnSyncOnHelper> sync_starter)
    : sync_starter_(sync_starter) {}

DiceTurnSyncOnHelper::SigninDialogDelegate::~SigninDialogDelegate() {}

void DiceTurnSyncOnHelper::SigninDialogDelegate::OnCancelSignin() {
  SetUserChoiceHistogram(SIGNIN_CHOICE_CANCEL);
  base::RecordAction(
      base::UserMetricsAction("Signin_EnterpriseAccountPrompt_Cancel"));

  if (sync_starter_)
    sync_starter_->AbortAndDelete();
}

void DiceTurnSyncOnHelper::SigninDialogDelegate::OnContinueSignin() {
  SetUserChoiceHistogram(SIGNIN_CHOICE_CONTINUE);
  base::RecordAction(
      base::UserMetricsAction("Signin_EnterpriseAccountPrompt_ImportData"));

  if (sync_starter_)
    sync_starter_->LoadPolicyWithCachedCredentials();
}

void DiceTurnSyncOnHelper::SigninDialogDelegate::OnSigninWithNewProfile() {
  SetUserChoiceHistogram(SIGNIN_CHOICE_NEW_PROFILE);
  base::RecordAction(
      base::UserMetricsAction("Signin_EnterpriseAccountPrompt_DontImportData"));

  if (sync_starter_)
    sync_starter_->CreateNewSignedInProfile();
}

DiceTurnSyncOnHelper::DiceTurnSyncOnHelper(
    Profile* profile,
    Browser* browser,
    signin_metrics::AccessPoint signin_access_point,
    signin_metrics::Reason signin_reason,
    const std::string& account_id,
    SigninAbortedMode signin_aborted_mode)
    : profile_(profile),
      browser_(browser),
      signin_manager_(SigninManagerFactory::GetForProfile(profile)),
      token_service_(ProfileOAuth2TokenServiceFactory::GetForProfile(profile)),
      signin_access_point_(signin_access_point),
      signin_reason_(signin_reason),
      signin_aborted_mode_(signin_aborted_mode),
      account_info_(GetAccountInfo(profile, account_id)),
      scoped_browser_list_observer_(this),
      scoped_login_ui_service_observer_(this),
      weak_pointer_factory_(this) {
  DCHECK(signin::IsDicePrepareMigrationEnabled());
  DCHECK(profile_);
  DCHECK(browser_);
  DCHECK(!account_info_.gaia.empty());
  DCHECK(!account_info_.email.empty());
  // Should not start syncing if the profile is already authenticated
  DCHECK(!signin_manager_->IsAuthenticated());

  // Force sign-in uses the modal sign-in flow.
  DCHECK(!signin_util::IsForceSigninEnabled());

  if (HasCanOfferSigninError()) {
    AbortAndDelete();
    return;
  }

  if (!IsCrossAccountError(profile_, account_info_.email, account_info_.gaia)) {
    TurnSyncOnWithProfileMode(ProfileMode::CURRENT_PROFILE);
    return;
  }

  // Handles cross account sign in error. If |account_info_| does not match the
  // last authenticated account of the current profile, then Chrome will show a
  // confirmation dialog before starting sync.
  // TODO(skym): Warn for high risk upgrade scenario (https://crbug.com/572754).
  content::WebContents* web_contents =
      browser_->tab_strip_model()->GetActiveWebContents();
  std::string last_email =
      profile_->GetPrefs()->GetString(prefs::kGoogleServicesLastUsername);
  SigninEmailConfirmationDialog::AskForConfirmation(
      web_contents, profile_, last_email, account_info_.email,
      base::Bind(&DiceTurnSyncOnHelper::ConfirmEmailAction,
                 weak_pointer_factory_.GetWeakPtr()));
}

DiceTurnSyncOnHelper::~DiceTurnSyncOnHelper() {
  DCHECK(!scoped_login_ui_service_observer_.IsObservingSources());
}

bool DiceTurnSyncOnHelper::HasCanOfferSigninError() {
  DCHECK(browser_);
  std::string error_msg;
  bool can_offer =
      CanOfferSignin(profile_, CAN_OFFER_SIGNIN_FOR_ALL_ACCOUNTS,
                     account_info_.gaia, account_info_.email, &error_msg);
  if (can_offer)
    return false;

  // Display the error message
  LoginUIServiceFactory::GetForProfile(profile_)->DisplayLoginResult(
      browser_, base::UTF8ToUTF16(error_msg),
      base::UTF8ToUTF16(account_info_.email));
  return true;
}

void DiceTurnSyncOnHelper::ConfirmEmailAction(
    SigninEmailConfirmationDialog::Action action) {
  switch (action) {
    case SigninEmailConfirmationDialog::CREATE_NEW_USER:
      base::RecordAction(
          base::UserMetricsAction("Signin_ImportDataPrompt_DontImport"));
      TurnSyncOnWithProfileMode(ProfileMode::NEW_PROFILE);
      break;
    case SigninEmailConfirmationDialog::START_SYNC:
      base::RecordAction(
          base::UserMetricsAction("Signin_ImportDataPrompt_ImportData"));
      TurnSyncOnWithProfileMode(ProfileMode::CURRENT_PROFILE);
      break;
    case SigninEmailConfirmationDialog::CLOSE:
      base::RecordAction(
          base::UserMetricsAction("Signin_ImportDataPrompt_Cancel"));
      AbortAndDelete();
      break;
  }
}

void DiceTurnSyncOnHelper::TurnSyncOnWithProfileMode(ProfileMode profile_mode) {
  scoped_browser_list_observer_.Add(BrowserList::GetInstance());

  // Make sure the syncing is requested, otherwise the SigninManager
  // will not be able to complete successfully.
  syncer::SyncPrefs sync_prefs(profile_->GetPrefs());
  sync_prefs.SetSyncRequested(true);

  switch (profile_mode) {
    case ProfileMode::CURRENT_PROFILE: {
      // If this is a new signin (no account authenticated yet) try loading
      // policy for this user now, before any signed in services are
      // initialized.
      policy::UserPolicySigninService* policy_service =
          policy::UserPolicySigninServiceFactory::GetForProfile(profile_);
      policy_service->RegisterForPolicyWithAccountId(
          account_info_.email, account_info_.account_id,
          base::Bind(&DiceTurnSyncOnHelper::OnRegisteredForPolicy,
                     weak_pointer_factory_.GetWeakPtr()));
      break;
    }
    case ProfileMode::NEW_PROFILE:
      // If this is a new signin (no account authenticated yet) in a new
      // profile, then just create the new signed-in profile and skip loading
      // the policy as there is no need to ask the user again if they should be
      // signed in to a new profile. Note that in this case the policy will be
      // applied after the new profile is signed in.
      CreateNewSignedInProfile();
      break;
  }
}

void DiceTurnSyncOnHelper::OnRegisteredForPolicy(const std::string& dm_token,
                                                 const std::string& client_id) {
  // If there's no token for the user (policy registration did not succeed) just
  // finish signing in.
  if (dm_token.empty()) {
    DVLOG(1) << "Policy registration failed";
    SigninAndShowSyncConfirmationUI();
    return;
  }

  DVLOG(1) << "Policy registration succeeded: dm_token=" << dm_token;

  DCHECK(dm_token_.empty());
  DCHECK(client_id_.empty());
  dm_token_ = dm_token;
  client_id_ = client_id;

  // Allow user to create a new profile before continuing with sign-in.
  browser_ = EnsureBrowser(browser_, profile_);
  content::WebContents* web_contents =
      browser_->tab_strip_model()->GetActiveWebContents();
  if (!web_contents) {
    AbortAndDelete();
    return;
  }

  base::RecordAction(
      base::UserMetricsAction("Signin_Show_EnterpriseAccountPrompt"));
  TabDialogs::FromWebContents(web_contents)
      ->ShowProfileSigninConfirmation(browser_, profile_, account_info_.email,
                                      std::make_unique<SigninDialogDelegate>(
                                          weak_pointer_factory_.GetWeakPtr()));
}

void DiceTurnSyncOnHelper::LoadPolicyWithCachedCredentials() {
  DCHECK(!dm_token_.empty());
  DCHECK(!client_id_.empty());
  policy::UserPolicySigninService* policy_service =
      policy::UserPolicySigninServiceFactory::GetForProfile(profile_);
  policy_service->FetchPolicyForSignedInUser(
      account_info_.email, dm_token_, client_id_, profile_->GetRequestContext(),
      base::Bind(&DiceTurnSyncOnHelper::OnPolicyFetchComplete,
                 weak_pointer_factory_.GetWeakPtr()));
}

void DiceTurnSyncOnHelper::OnPolicyFetchComplete(bool success) {
  // For now, we allow signin to complete even if the policy fetch fails. If
  // we ever want to change this behavior, we could call
  // SigninManager::SignOut() here instead.
  DLOG_IF(ERROR, !success) << "Error fetching policy for user";
  DVLOG_IF(1, success) << "Policy fetch successful - completing signin";
  SigninAndShowSyncConfirmationUI();
}

void DiceTurnSyncOnHelper::CreateNewSignedInProfile() {
  // Create a new profile and have it call back when done so we can start the
  // signin flow.
  size_t icon_index = g_browser_process->profile_manager()
                          ->GetProfileAttributesStorage()
                          .ChooseAvatarIconIndexForNewProfile();
  ProfileManager::CreateMultiProfileAsync(
      base::UTF8ToUTF16(account_info_.email),
      profiles::GetDefaultAvatarIconUrl(icon_index),
      base::BindRepeating(&DiceTurnSyncOnHelper::CompleteInitForNewProfile,
                          weak_pointer_factory_.GetWeakPtr()),
      std::string());
}

void DiceTurnSyncOnHelper::CompleteInitForNewProfile(
    Profile* new_profile,
    Profile::CreateStatus status) {
  DCHECK_NE(profile_, new_profile);

  // TODO(atwilson): On error, unregister the client to release the DMToken
  // and surface a better error for the user.
  switch (status) {
    case Profile::CREATE_STATUS_LOCAL_FAIL:
      NOTREACHED() << "Error creating new profile";
      AbortAndDelete();
      break;
    case Profile::CREATE_STATUS_CREATED:
      // Ignore this, wait for profile to be initialized.
      break;
    case Profile::CREATE_STATUS_INITIALIZED:
      // The user needs to sign in to the new profile in order to enable sync.
      StartNewSigninInNewProfile(new_profile, account_info_.email);
      AbortAndDelete();
      break;
    case Profile::CREATE_STATUS_REMOTE_FAIL:
    case Profile::CREATE_STATUS_CANCELED:
    case Profile::MAX_CREATE_STATUS: {
      NOTREACHED() << "Invalid profile creation status";
      AbortAndDelete();
      break;
    }
  }
}

browser_sync::ProfileSyncService*
DiceTurnSyncOnHelper::GetProfileSyncService() {
  return profile_->IsSyncAllowed()
             ? ProfileSyncServiceFactory::GetForProfile(profile_)
             : nullptr;
}

void DiceTurnSyncOnHelper::SigninAndShowSyncConfirmationUI() {
  // Signin.
  signin_manager_->OnExternalSigninCompleted(account_info_.email);
  signin_metrics::LogSigninAccessPointCompleted(signin_access_point_);
  signin_metrics::LogSigninReason(signin_reason_);
  base::RecordAction(base::UserMetricsAction("Signin_Signin_Succeed"));

  // Show Sync confirmation.
  browser_sync::ProfileSyncService* sync_service = GetProfileSyncService();
  if (sync_service)
    sync_blocker_ = sync_service->GetSetupInProgressHandle();
  scoped_login_ui_service_observer_.Add(
      LoginUIServiceFactory::GetForProfile(profile_));
  browser_ = EnsureBrowser(browser_, profile_);
  browser_->signin_view_controller()->ShowModalSyncConfirmationDialog(browser_);
}

void DiceTurnSyncOnHelper::OnSyncConfirmationUIClosed(
    LoginUIService::SyncConfirmationUIClosedResult result) {
  scoped_login_ui_service_observer_.RemoveAll();
  switch (result) {
    case LoginUIService::CONFIGURE_SYNC_FIRST:
      browser_ = EnsureBrowser(browser_, profile_);
      chrome::ShowSettingsSubPage(browser_, chrome::kSyncSetupSubPage);
      break;
    case LoginUIService::SYNC_WITH_DEFAULT_SETTINGS: {
      browser_sync::ProfileSyncService* sync_service = GetProfileSyncService();
      if (sync_service)
        sync_service->SetFirstSetupComplete();
      break;
    }
    case LoginUIService::ABORT_SIGNIN:
      signin_manager_->SignOutAndKeepAllAccounts(
          signin_metrics::ABORT_SIGNIN,
          signin_metrics::SignoutDelete::IGNORE_METRIC);
      AbortAndDelete();
      return;
  }
  delete this;
}

void DiceTurnSyncOnHelper::OnBrowserRemoved(Browser* browser) {
  if (browser == browser_)
    browser_ = nullptr;
}

void DiceTurnSyncOnHelper::AbortAndDelete() {
  if (signin_aborted_mode_ == SigninAbortedMode::REMOVE_ACCOUNT) {
    // Revoke the token, and the AccountReconcilor and/or the Gaia server will
    // take care of invalidating the cookies.
    token_service_->RevokeCredentials(account_info_.account_id);
  }
  delete this;
}
