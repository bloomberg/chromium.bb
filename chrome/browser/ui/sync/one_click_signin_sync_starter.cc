// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/sync/one_click_signin_sync_starter.h"

#include <stddef.h>

#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service_factory.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_tracker_factory.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/tab_dialogs.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/browser/unified_consent/unified_consent_service_factory.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/signin_metrics.h"
#include "components/sync/base/sync_prefs.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_user_settings.h"
#include "components/unified_consent/feature.h"
#include "components/unified_consent/unified_consent_service.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/url_util.h"
#include "services/identity/public/cpp/identity_manager.h"
#include "services/identity/public/cpp/primary_account_mutator.h"
#include "ui/base/l10n/l10n_util.h"

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
  UMA_HISTOGRAM_ENUMERATION("Enterprise.UserSigninChoice",
                            choice,
                            SIGNIN_CHOICE_SIZE);
}

}  // namespace

OneClickSigninSyncStarter::OneClickSigninSyncStarter(
    Profile* profile,
    Browser* browser,
    const std::string& gaia_id,
    const std::string& email,
    const std::string& password,
    const std::string& refresh_token,
    signin_metrics::AccessPoint signin_access_point,
    signin_metrics::Reason signin_reason,
    ProfileMode profile_mode,
    Callback sync_setup_completed_callback)
    : profile_(nullptr),
      signin_access_point_(signin_access_point),
      signin_reason_(signin_reason),
      sync_setup_completed_callback_(sync_setup_completed_callback),
      first_account_added_to_cookie_(false),
      weak_pointer_factory_(this) {
  DCHECK(profile);
  BrowserList::AddObserver(this);
  Initialize(profile, browser);
  DCHECK(!refresh_token.empty());

  DCHECK(primary_account_mutator_);
  primary_account_mutator_->LegacyStartSigninWithRefreshTokenForPrimaryAccount(
      refresh_token, gaia_id, email, password,
      base::BindOnce(&OneClickSigninSyncStarter::ConfirmSignin,
                     weak_pointer_factory_.GetWeakPtr(), profile_mode));
}

void OneClickSigninSyncStarter::OnBrowserRemoved(Browser* browser) {
  if (browser == browser_)
    browser_ = nullptr;
}

OneClickSigninSyncStarter::~OneClickSigninSyncStarter() {
  BrowserList::RemoveObserver(this);
  LoginUIServiceFactory::GetForProfile(profile_)->RemoveObserver(this);
}

void OneClickSigninSyncStarter::Initialize(Profile* profile, Browser* browser) {
  DCHECK(profile);

  if (profile_)
    LoginUIServiceFactory::GetForProfile(profile_)->RemoveObserver(this);

  profile_ = profile;
  browser_ = browser;

  LoginUIServiceFactory::GetForProfile(profile_)->AddObserver(this);

  signin_tracker_ = SigninTrackerFactory::CreateForProfile(profile_, this);

  // Let the sync service know that setup is in progress so it doesn't start
  // syncing until the user has finished any configuration.
  syncer::SyncService* sync_service = GetSyncService();
  if (sync_service)
    sync_blocker_ = sync_service->GetSetupInProgressHandle();

  // Make sure the syncing is requested, otherwise the IdentityManager's primary
  // account mutator will not be able to complete successfully.
  syncer::SyncPrefs sync_prefs(profile_->GetPrefs());
  sync_prefs.SetSyncRequested(true);

  // Cache the IdentityManager's PrimaryAccountMutator each time the profile
  // gets changed, as it will be used from multiple places in several methods.
  primary_account_mutator_ = IdentityManagerFactory::GetForProfile(profile_)
                                 ->GetPrimaryAccountMutator();
}

void OneClickSigninSyncStarter::ConfirmSignin(ProfileMode profile_mode,
                                              const std::string& oauth_token) {
  identity::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_);

  if (identity_manager->HasPrimaryAccount()) {
    // The user is already signed in - just tell IdentityManager's primary
    // account mutator to continue with its re-auth flow.
    DCHECK_EQ(CURRENT_PROFILE, profile_mode);
    primary_account_mutator_->LegacyCompletePendingPrimaryAccountSignin();
    return;
  }

  switch (profile_mode) {
    case CURRENT_PROFILE: {
      // If this is a new signin (no account authenticated yet) try loading
      // policy for this user now, before any signed in services are
      // initialized.
      policy::UserPolicySigninService* policy_service =
          policy::UserPolicySigninServiceFactory::GetForProfile(profile_);
      policy_service->RegisterForPolicyWithLoginToken(
          primary_account_mutator_->LegacyPrimaryAccountForAuthInProgress()
              .email,
          oauth_token,
          base::Bind(&OneClickSigninSyncStarter::OnRegisteredForPolicy,
                     weak_pointer_factory_.GetWeakPtr()));
      break;
    }
    case NEW_PROFILE:
      // If this is a new signin (no account authenticated yet) in a new
      // profile, then just create the new signed-in profile and skip loading
      // the policy as there is no need to ask the user again if they should be
      // signed in to a new profile. Note that in this case the policy will be
      // applied after the new profile is signed in.
      CreateNewSignedInProfile();
      break;
  }
}

OneClickSigninSyncStarter::SigninDialogDelegate::SigninDialogDelegate(
    base::WeakPtr<OneClickSigninSyncStarter> sync_starter)
    : sync_starter_(sync_starter) {
}

OneClickSigninSyncStarter::SigninDialogDelegate::~SigninDialogDelegate() {
}

void OneClickSigninSyncStarter::SigninDialogDelegate::OnCancelSignin() {
  SetUserChoiceHistogram(SIGNIN_CHOICE_CANCEL);
  base::RecordAction(
      base::UserMetricsAction("Signin_EnterpriseAccountPrompt_Cancel"));
  if (sync_starter_)
    sync_starter_->CancelSigninAndDelete();
}

void OneClickSigninSyncStarter::SigninDialogDelegate::OnContinueSignin() {
  SetUserChoiceHistogram(SIGNIN_CHOICE_CONTINUE);
  base::RecordAction(
      base::UserMetricsAction("Signin_EnterpriseAccountPrompt_ImportData"));

  if (sync_starter_)
    sync_starter_->LoadPolicyWithCachedCredentials();
}

void OneClickSigninSyncStarter::SigninDialogDelegate::OnSigninWithNewProfile() {
  SetUserChoiceHistogram(SIGNIN_CHOICE_NEW_PROFILE);
  base::RecordAction(
      base::UserMetricsAction("Signin_EnterpriseAccountPrompt_DontImportData"));

  if (sync_starter_)
    sync_starter_->CreateNewSignedInProfile();
}

void OneClickSigninSyncStarter::OnRegisteredForPolicy(
    const std::string& dm_token, const std::string& client_id) {
  // If there's no token for the user (policy registration did not succeed) just
  // finish signing in.
  if (dm_token.empty()) {
    DVLOG(1) << "Policy registration failed";
    primary_account_mutator_->LegacyCompletePendingPrimaryAccountSignin();
    return;
  }

  DVLOG(1) << "Policy registration succeeded: dm_token=" << dm_token;

  // Stash away a copy of our CloudPolicyClient (should not already have one).
  DCHECK(dm_token_.empty());
  DCHECK(client_id_.empty());
  dm_token_ = dm_token;
  client_id_ = client_id;

  if (signin_util::IsForceSigninEnabled()) {
    LoadPolicyWithCachedCredentials();
    return;
  }

  // Allow user to create a new profile before continuing with sign-in.
  browser_ = EnsureBrowser(browser_, profile_);
  content::WebContents* web_contents =
      browser_->tab_strip_model()->GetActiveWebContents();
  if (!web_contents) {
    CancelSigninAndDelete();
    return;
  }

  base::RecordAction(
      base::UserMetricsAction("Signin_Show_EnterpriseAccountPrompt"));

  TabDialogs::FromWebContents(web_contents)
      ->ShowProfileSigninConfirmation(
          browser_, profile_,
          primary_account_mutator_->LegacyPrimaryAccountForAuthInProgress()
              .email,
          std::make_unique<SigninDialogDelegate>(
              weak_pointer_factory_.GetWeakPtr()));
}

void OneClickSigninSyncStarter::LoadPolicyWithCachedCredentials() {
  DCHECK(!dm_token_.empty());
  DCHECK(!client_id_.empty());
  policy::UserPolicySigninService* policy_service =
      policy::UserPolicySigninServiceFactory::GetForProfile(profile_);

  std::string username =
      primary_account_mutator_->LegacyPrimaryAccountForAuthInProgress().email;
  std::string gaia_id =
      primary_account_mutator_->LegacyPrimaryAccountForAuthInProgress().gaia;
  DCHECK(username.empty() == gaia_id.empty());
  AccountId account_id =
      username.empty() ? EmptyAccountId()
                       : AccountId::FromUserEmailGaiaId(username, gaia_id);
  policy_service->FetchPolicyForSignedInUser(
      account_id, dm_token_, client_id_,
      content::BrowserContext::GetDefaultStoragePartition(profile_)
          ->GetURLLoaderFactoryForBrowserProcess(),
      base::Bind(&OneClickSigninSyncStarter::OnPolicyFetchComplete,
                 weak_pointer_factory_.GetWeakPtr()));
}

void OneClickSigninSyncStarter::OnPolicyFetchComplete(bool success) {
  // For now, we allow signin to complete even if the policy fetch fails. If
  // we ever want to change this behavior, we could call
  // PrimaryAccountMutator::ClearPrimaryAccount() here instead.
  DLOG_IF(ERROR, !success) << "Error fetching policy for user";
  DVLOG_IF(1, success) << "Policy fetch successful - completing signin";

  primary_account_mutator_->LegacyCompletePendingPrimaryAccountSignin();
}

void OneClickSigninSyncStarter::CreateNewSignedInProfile() {
  const std::string email =
      primary_account_mutator_->LegacyPrimaryAccountForAuthInProgress().email;
  DCHECK(!email.empty());

  // Create a new profile and have it call back when done so we can inject our
  // signin credentials.
  size_t icon_index = g_browser_process->profile_manager()->
      GetProfileAttributesStorage().ChooseAvatarIconIndexForNewProfile();
  ProfileManager::CreateMultiProfileAsync(
      base::UTF8ToUTF16(email), profiles::GetDefaultAvatarIconUrl(icon_index),
      base::Bind(&OneClickSigninSyncStarter::CompleteInitForNewProfile,
                 weak_pointer_factory_.GetWeakPtr()));
}

void OneClickSigninSyncStarter::CompleteInitForNewProfile(
    Profile* new_profile,
    Profile::CreateStatus status) {
  DCHECK_NE(profile_, new_profile);

  // TODO(atwilson): On error, unregister the client to release the DMToken
  // and surface a better error for the user.
  switch (status) {
    case Profile::CREATE_STATUS_LOCAL_FAIL: {
      NOTREACHED() << "Error creating new profile";
      CancelSigninAndDelete();
      return;
    }
    case Profile::CREATE_STATUS_CREATED: {
      break;
    }
    case Profile::CREATE_STATUS_INITIALIZED:
      // Pre-DICE, the refresh token is copied to the new profile and the user
      // does not need to autehnticate in the new profile.
      CopyCredentialsToNewProfileAndFinishSignin(new_profile);
      break;
    case Profile::CREATE_STATUS_REMOTE_FAIL:
    case Profile::CREATE_STATUS_CANCELED:
    case Profile::MAX_CREATE_STATUS: {
      NOTREACHED() << "Invalid profile creation status";
      CancelSigninAndDelete();
      return;
    }
  }
}

void OneClickSigninSyncStarter::CopyCredentialsToNewProfileAndFinishSignin(
    Profile* new_profile) {
  // Wait until the profile is initialized before we transfer credentials.
  identity::IdentityManager* new_identity_manager =
      IdentityManagerFactory::GetForProfile(new_profile);
  auto* new_primary_account_mutator =
      new_identity_manager->GetPrimaryAccountMutator();
  DCHECK(new_primary_account_mutator);

  DCHECK(!primary_account_mutator_->LegacyPrimaryAccountForAuthInProgress()
              .email.empty());

  identity::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_);
  DCHECK(!identity_manager->HasPrimaryAccount());
  DCHECK(!new_identity_manager->HasPrimaryAccount());

  // Copy credentials from the old profile to the just-created profile,
  // and switch over to tracking that profile.
  new_primary_account_mutator->LegacyCopyCredentialsFrom(
      *primary_account_mutator_);
  FinishSyncServiceSetup();
  Initialize(new_profile, nullptr);
  DCHECK_EQ(profile_, new_profile);

  // We've transferred our credentials to the new profile - notify that
  // the signin for the original profile was cancelled (must do this after
  // we have called Initialize() with the new profile, as otherwise this
  // object will get freed when the signin on the old profile is cancelled.
  // ClearPrimaryAccount does not actually remove the accounts if the
  // signin is still pending. See http://crbug.com/799437.
  primary_account_mutator_->ClearPrimaryAccount(
      identity::PrimaryAccountMutator::ClearAccountsAction::kRemoveAll,
      signin_metrics::TRANSFER_CREDENTIALS,
      signin_metrics::SignoutDelete::IGNORE_METRIC);

  if (!dm_token_.empty()) {
    // Load policy for the just-created profile - once policy has finished
    // loading the signin process will complete.
    DCHECK(!client_id_.empty());
    LoadPolicyWithCachedCredentials();
  } else {
    // No policy to load - simply complete the signin process.
    primary_account_mutator_->LegacyCompletePendingPrimaryAccountSignin();
  }

  // Unlock the new profile.
  ProfileAttributesEntry* entry;
  bool has_entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile_->GetPath(), &entry);
  DCHECK(has_entry);
  entry->SetIsSigninRequired(false);

  // Open the profile's first window, after all initialization.
  profiles::FindOrCreateNewWindowForProfile(
      new_profile, chrome::startup::IS_PROCESS_STARTUP,
      chrome::startup::IS_FIRST_RUN, false);
}

void OneClickSigninSyncStarter::CancelSigninAndDelete() {
  DCHECK(primary_account_mutator_->LegacyIsPrimaryAccountAuthInProgress());

  // ClearPrimaryAccount does not actually remove the accounts if the
  // signin is still pending. See http://crbug.com/799437.
  primary_account_mutator_->ClearPrimaryAccount(
      identity::PrimaryAccountMutator::ClearAccountsAction::kRemoveAll,
      signin_metrics::ABORT_SIGNIN,
      signin_metrics::SignoutDelete::IGNORE_METRIC);
  // The statement above results in a call to SigninFailed() which will free
  // this object, so do not refer to the OneClickSigninSyncStarter object
  // after this point.
}

void OneClickSigninSyncStarter::OnSyncConfirmationUIClosed(
    LoginUIService::SyncConfirmationUIClosedResult result) {
  // We didn't run this callback in AccountAddedToCookie so do it now.
  if (!sync_setup_completed_callback_.is_null())
    sync_setup_completed_callback_.Run(SYNC_SETUP_SUCCESS);

  unified_consent::UnifiedConsentService* consent_service =
      UnifiedConsentServiceFactory::GetForProfile(profile_);

  switch (result) {
    case LoginUIService::CONFIGURE_SYNC_FIRST:
      if (consent_service)
        consent_service->EnableGoogleServices();
      ShowSyncSetupSettingsSubpage();
      break;
    case LoginUIService::SYNC_WITH_DEFAULT_SETTINGS: {
      syncer::SyncService* sync_service = GetSyncService();
      if (sync_service)
        sync_service->GetUserSettings()->SetFirstSetupComplete();
      if (consent_service)
        consent_service->EnableGoogleServices();
      FinishSyncServiceSetup();
      break;
    }
    case LoginUIService::ABORT_SIGNIN:
      primary_account_mutator_->ClearPrimaryAccount(
          identity::PrimaryAccountMutator::ClearAccountsAction::kDefault,
          signin_metrics::ABORT_SIGNIN,
          signin_metrics::SignoutDelete::IGNORE_METRIC);
      FinishSyncServiceSetup();
      break;
  }

  delete this;
}

void OneClickSigninSyncStarter::SigninFailed(
    const GoogleServiceAuthError& error) {
  if (!sync_setup_completed_callback_.is_null())
    sync_setup_completed_callback_.Run(SYNC_SETUP_FAILURE);

  FinishSyncServiceSetup();
  switch (error.state()) {
    case GoogleServiceAuthError::SERVICE_UNAVAILABLE:
      DisplayFinalConfirmationBubble(
          l10n_util::GetStringUTF16(IDS_SYNC_UNRECOVERABLE_ERROR));
      break;
    case GoogleServiceAuthError::REQUEST_CANCELED:
      // No error notification needed if the user manually cancelled signin.
      break;
    default:
      DisplayFinalConfirmationBubble(
          l10n_util::GetStringUTF16(IDS_SYNC_ERROR_SIGNING_IN));
      break;
  }
  delete this;
}

void OneClickSigninSyncStarter::SigninSuccess() {
  signin_metrics::LogSigninAccessPointCompleted(
      signin_access_point_,
      signin_metrics::PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO);
  signin_metrics::LogSigninReason(signin_reason_);
  base::RecordAction(base::UserMetricsAction("Signin_Signin_Succeed"));
}

void OneClickSigninSyncStarter::AccountAddedToCookie(
    const GoogleServiceAuthError& error) {
  if (first_account_added_to_cookie_)
    return;

  first_account_added_to_cookie_ = true;

  // Regardless of whether the account was successfully added or not,
  // continue with sync starting.

  // |sync_setup_completed_callback_| will be run after the modal is closed.
  DisplayModalSyncConfirmationWindow();
}

void OneClickSigninSyncStarter::DisplayFinalConfirmationBubble(
    const base::string16& custom_message) {
  browser_ = EnsureBrowser(browser_, profile_);
  LoginUIServiceFactory::GetForProfile(browser_->profile())
      ->DisplayLoginResult(browser_, custom_message, base::string16());
}

void OneClickSigninSyncStarter::DisplayModalSyncConfirmationWindow() {
  browser_ = EnsureBrowser(browser_, profile_);
  browser_->signin_view_controller()->ShowModalSyncConfirmationDialog(browser_);
}

// static
Browser* OneClickSigninSyncStarter::EnsureBrowser(Browser* browser,
                                                  Profile* profile) {
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

void OneClickSigninSyncStarter::ShowSyncSetupSettingsSubpage() {
  chrome::ShowSettingsSubPage(browser_, chrome::kSyncSetupSubPage);
}

syncer::SyncService* OneClickSigninSyncStarter::GetSyncService() {
  if (!profile_->IsSyncAllowed())
    return nullptr;
  return ProfileSyncServiceFactory::GetSyncServiceForProfile(profile_);
}

void OneClickSigninSyncStarter::FinishSyncServiceSetup() {
  sync_blocker_.reset();
}
