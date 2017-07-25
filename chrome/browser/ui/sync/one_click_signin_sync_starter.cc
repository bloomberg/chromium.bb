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
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/signin/signin_tracker_factory.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/sync/one_click_signin_sync_observer.h"
#include "chrome/browser/ui/tab_dialogs.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/signin/core/browser/signin_metrics.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "components/sync/base/sync_prefs.h"
#include "net/base/url_util.h"
#include "net/url_request/url_request_context_getter.h"
#include "ui/base/l10n/l10n_util.h"

using browser_sync::ProfileSyncService;

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

// Lock the profile in memory when |enable| is false. Or unlock the profile
// if |enable| is true. This is used by force-sign-in policy only.
void EnableProfileForForceSigninInMemory(Profile* profile, bool enable) {
  ProfileAttributesEntry* entry;
  bool has_entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile->GetPath(), &entry);
  DCHECK(has_entry);
  if (enable) {
    entry->SetIsSigninRequired(false);
  } else {
    entry->LockForceSigninProfile(true);
  }
}

}  // namespace

OneClickSigninSyncStarter::OneClickSigninSyncStarter(
    Profile* profile,
    Browser* browser,
    const std::string& gaia_id,
    const std::string& email,
    const std::string& password,
    const std::string& refresh_token,
    ProfileMode profile_mode,
    StartSyncMode start_mode,
    content::WebContents* web_contents,
    ConfirmationRequired confirmation_required,
    const GURL& current_url,
    const GURL& continue_url,
    Callback sync_setup_completed_callback)
    : content::WebContentsObserver(web_contents),
      profile_(nullptr),
      start_mode_(start_mode),
      confirmation_required_(confirmation_required),
      current_url_(current_url),
      continue_url_(continue_url),
      sync_setup_completed_callback_(sync_setup_completed_callback),
      first_account_added_to_cookie_(false),
      weak_pointer_factory_(this) {
  DCHECK(profile);
  DCHECK(web_contents || continue_url.is_empty());
  BrowserList::AddObserver(this);
  Initialize(profile, browser);

  DCHECK(!refresh_token.empty() || signin::IsAccountConsistencyDiceEnabled());
  SigninManagerFactory::GetForProfile(profile_)->StartSignInWithRefreshToken(
      refresh_token, gaia_id, email, password,
      base::Bind(&OneClickSigninSyncStarter::ConfirmSignin,
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
  ProfileSyncService* profile_sync_service = GetProfileSyncService();
  if (profile_sync_service)
    sync_blocker_ = profile_sync_service->GetSetupInProgressHandle();

  // Make sure the syncing is requested, otherwise the SigninManager
  // will not be able to complete successfully.
  syncer::SyncPrefs sync_prefs(profile_->GetPrefs());
  sync_prefs.SetSyncRequested(true);
}

void OneClickSigninSyncStarter::ConfirmSignin(ProfileMode profile_mode,
                                              const std::string& oauth_token) {
  SigninManager* signin = SigninManagerFactory::GetForProfile(profile_);
  if (signin->IsAuthenticated()) {
    // The user is already signed in - just tell SigninManager to continue
    // with its re-auth flow.
    DCHECK_EQ(CURRENT_PROFILE, profile_mode);
    signin->CompletePendingSignin();
    return;
  }

  switch (profile_mode) {
    case CURRENT_PROFILE: {
      // If this is a new signin (no account authenticated yet) try loading
      // policy for this user now, before any signed in services are
      // initialized.
      policy::UserPolicySigninService* policy_service =
          policy::UserPolicySigninServiceFactory::GetForProfile(profile_);
      if (oauth_token.empty()) {
        DCHECK(signin::IsAccountConsistencyDiceEnabled());
        policy_service->RegisterForPolicyWithAccountId(
            signin->GetUsernameForAuthInProgress(),
            signin->GetAccountIdForAuthInProgress(),
            base::Bind(&OneClickSigninSyncStarter::OnRegisteredForPolicy,
                       weak_pointer_factory_.GetWeakPtr()));
      } else {
        policy_service->RegisterForPolicyWithLoginToken(
            signin->GetUsernameForAuthInProgress(), oauth_token,
            base::Bind(&OneClickSigninSyncStarter::OnRegisteredForPolicy,
                       weak_pointer_factory_.GetWeakPtr()));
      }
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
  SigninManager* signin = SigninManagerFactory::GetForProfile(profile_);
  // If there's no token for the user (policy registration did not succeed) just
  // finish signing in.
  if (dm_token.empty()) {
    DVLOG(1) << "Policy registration failed";
    ConfirmAndSignin();
    return;
  }

  DVLOG(1) << "Policy registration succeeded: dm_token=" << dm_token;

  // Stash away a copy of our CloudPolicyClient (should not already have one).
  DCHECK(dm_token_.empty());
  DCHECK(client_id_.empty());
  dm_token_ = dm_token;
  client_id_ = client_id;

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
      ->ShowProfileSigninConfirmation(browser_, profile_,
                                      signin->GetUsernameForAuthInProgress(),
                                      base::MakeUnique<SigninDialogDelegate>(
                                          weak_pointer_factory_.GetWeakPtr()));
  // If force signin enabled, lock the profile when dialog is being displayed to
  // avoid new browser window opened.
  if (signin_util::IsForceSigninEnabled())
    EnableProfileForForceSigninInMemory(profile_, false);
}

void OneClickSigninSyncStarter::LoadPolicyWithCachedCredentials() {
  DCHECK(!dm_token_.empty());
  DCHECK(!client_id_.empty());
  SigninManager* signin = SigninManagerFactory::GetForProfile(profile_);
  policy::UserPolicySigninService* policy_service =
      policy::UserPolicySigninServiceFactory::GetForProfile(profile_);
  policy_service->FetchPolicyForSignedInUser(
      signin->GetUsernameForAuthInProgress(),
      dm_token_,
      client_id_,
      profile_->GetRequestContext(),
      base::Bind(&OneClickSigninSyncStarter::OnPolicyFetchComplete,
                 weak_pointer_factory_.GetWeakPtr()));

  // Unlock the profile after loading policies.
  EnableProfileForForceSigninInMemory(profile_, true);
}

void OneClickSigninSyncStarter::OnPolicyFetchComplete(bool success) {
  // For now, we allow signin to complete even if the policy fetch fails. If
  // we ever want to change this behavior, we could call
  // SigninManager::SignOut() here instead.
  DLOG_IF(ERROR, !success) << "Error fetching policy for user";
  DVLOG_IF(1, success) << "Policy fetch successful - completing signin";
  SigninManagerFactory::GetForProfile(profile_)->CompletePendingSignin();
}

void OneClickSigninSyncStarter::CreateNewSignedInProfile() {
  SigninManager* signin = SigninManagerFactory::GetForProfile(profile_);
  DCHECK(!signin->GetUsernameForAuthInProgress().empty());

  // Create a new profile and have it call back when done so we can inject our
  // signin credentials.
  size_t icon_index = g_browser_process->profile_manager()->
      GetProfileAttributesStorage().ChooseAvatarIconIndexForNewProfile();
  ProfileManager::CreateMultiProfileAsync(
      base::UTF8ToUTF16(signin->GetUsernameForAuthInProgress()),
      profiles::GetDefaultAvatarIconUrl(icon_index),
      base::Bind(&OneClickSigninSyncStarter::CompleteInitForNewProfile,
                 weak_pointer_factory_.GetWeakPtr()),
      std::string());
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
    case Profile::CREATE_STATUS_INITIALIZED: {
      // Wait until the profile is initialized before we transfer credentials.
      SigninManager* old_signin_manager =
          SigninManagerFactory::GetForProfile(profile_);
      SigninManager* new_signin_manager =
          SigninManagerFactory::GetForProfile(new_profile);
      DCHECK(!old_signin_manager->GetUsernameForAuthInProgress().empty());
      DCHECK(!old_signin_manager->IsAuthenticated());
      DCHECK(!new_signin_manager->IsAuthenticated());

      // Copy credentials from the old profile to the just-created profile,
      // and switch over to tracking that profile.
      new_signin_manager->CopyCredentialsFrom(*old_signin_manager);
      FinishProfileSyncServiceSetup();
      Initialize(new_profile, nullptr);
      DCHECK_EQ(profile_, new_profile);

      // We've transferred our credentials to the new profile - notify that
      // the signin for the original profile was cancelled (must do this after
      // we have called Initialize() with the new profile, as otherwise this
      // object will get freed when the signin on the old profile is cancelled.
      old_signin_manager->SignOut(signin_metrics::TRANSFER_CREDENTIALS,
                                  signin_metrics::SignoutDelete::IGNORE_METRIC);

      if (!dm_token_.empty()) {
        // Load policy for the just-created profile - once policy has finished
        // loading the signin process will complete.
        DCHECK(!client_id_.empty());
        LoadPolicyWithCachedCredentials();
      } else {
        // No policy to load - simply complete the signin process and unlock the
        // profile.
        SigninManagerFactory::GetForProfile(profile_)->CompletePendingSignin();
        EnableProfileForForceSigninInMemory(profile_, true);
      }

      // Open the profile's first window, after all initialization.
      profiles::FindOrCreateNewWindowForProfile(
        new_profile,
        chrome::startup::IS_PROCESS_STARTUP,
        chrome::startup::IS_FIRST_RUN,
        false);
      break;
    }
    case Profile::CREATE_STATUS_REMOTE_FAIL:
    case Profile::CREATE_STATUS_CANCELED:
    case Profile::MAX_CREATE_STATUS: {
      NOTREACHED() << "Invalid profile creation status";
      CancelSigninAndDelete();
      return;
    }
  }
}

void OneClickSigninSyncStarter::CancelSigninAndDelete() {
  SigninManagerFactory::GetForProfile(profile_)
      ->SignOut(signin_metrics::ABORT_SIGNIN,
                signin_metrics::SignoutDelete::IGNORE_METRIC);
  // The statement above results in a call to SigninFailed() which will free
  // this object, so do not refer to the OneClickSigninSyncStarter object
  // after this point.
}

void OneClickSigninSyncStarter::ConfirmAndSignin() {
  SigninManager* signin = SigninManagerFactory::GetForProfile(profile_);
  if (confirmation_required_ == CONFIRM_UNTRUSTED_SIGNIN) {
    browser_ = EnsureBrowser(browser_, profile_);
    base::RecordAction(
        base::UserMetricsAction("Signin_Show_UntrustedSigninPrompt"));
    // Display a confirmation dialog to the user.
    browser_->window()->ShowOneClickSigninConfirmation(
        base::UTF8ToUTF16(signin->GetUsernameForAuthInProgress()),
        base::Bind(&OneClickSigninSyncStarter::UntrustedSigninConfirmed,
                   weak_pointer_factory_.GetWeakPtr()));
  } else {
    // No confirmation required - just sign in the user.
    signin->CompletePendingSignin();
  }
}

void OneClickSigninSyncStarter::UntrustedSigninConfirmed(
    StartSyncMode response) {
  if (response == UNDO_SYNC) {
    base::RecordAction(base::UserMetricsAction("Signin_Undo_Signin"));
    CancelSigninAndDelete();  // This statement frees this object.
  } else {
    // If the user clicked the "Advanced" link in the confirmation dialog, then
    // override the current start_mode_ to bring up the advanced sync settings.

    // If the user signs in from the new avatar bubble, the untrusted dialog
    // would dismiss the avatar bubble, thus it won't show any confirmation upon
    // sign in completes. This dialog already has a settings link, thus we just
    // start sync immediately .

    if (response == CONFIGURE_SYNC_FIRST)
      start_mode_ = response;
    else if (start_mode_ == CONFIRM_SYNC_SETTINGS_FIRST)
      start_mode_ = SYNC_WITH_DEFAULT_SETTINGS;

    SigninManager* signin = SigninManagerFactory::GetForProfile(profile_);
    signin->CompletePendingSignin();
  }
}

void OneClickSigninSyncStarter::OnSyncConfirmationUIClosed(
    LoginUIService::SyncConfirmationUIClosedResult result) {
  // We didn't run this callback in AccountAddedToCookie so do it now.
  if (!sync_setup_completed_callback_.is_null())
    sync_setup_completed_callback_.Run(SYNC_SETUP_SUCCESS);

  switch (result) {
    case LoginUIService::CONFIGURE_SYNC_FIRST:
      base::RecordAction(
          base::UserMetricsAction("Signin_Signin_WithAdvancedSyncSettings"));
      ShowSyncSetupSettingsSubpage();
      break;
    case LoginUIService::SYNC_WITH_DEFAULT_SETTINGS: {
      base::RecordAction(
          base::UserMetricsAction("Signin_Signin_WithDefaultSyncSettings"));
      ProfileSyncService* profile_sync_service = GetProfileSyncService();
      if (profile_sync_service)
        profile_sync_service->SetFirstSetupComplete();
      FinishProfileSyncServiceSetup();
      break;
    }
    case LoginUIService::ABORT_SIGNIN:
      FinishProfileSyncServiceSetup();
      break;
  }

  delete this;
}

void OneClickSigninSyncStarter::SigninFailed(
    const GoogleServiceAuthError& error) {
  if (!sync_setup_completed_callback_.is_null())
    sync_setup_completed_callback_.Run(SYNC_SETUP_FAILURE);

  FinishProfileSyncServiceSetup();
  if (confirmation_required_ == CONFIRM_AFTER_SIGNIN) {
    switch (error.state()) {
      case GoogleServiceAuthError::SERVICE_UNAVAILABLE:
        DisplayFinalConfirmationBubble(l10n_util::GetStringUTF16(
            IDS_SYNC_UNRECOVERABLE_ERROR));
        break;
      case GoogleServiceAuthError::REQUEST_CANCELED:
        // No error notification needed if the user manually cancelled signin.
        break;
      default:
        DisplayFinalConfirmationBubble(l10n_util::GetStringUTF16(
            IDS_SYNC_ERROR_SIGNING_IN));
        break;
    }
  }
  delete this;
}

void OneClickSigninSyncStarter::SigninSuccess() {
  if (!current_url_.is_valid())  // Could be invalid for tests.
    return;
  signin_metrics::LogSigninAccessPointCompleted(
      signin::GetAccessPointForPromoURL(current_url_));
  signin_metrics::LogSigninReason(
      signin::GetSigninReasonForPromoURL(current_url_));
  base::RecordAction(base::UserMetricsAction("Signin_Signin_Succeed"));
}

void OneClickSigninSyncStarter::AccountAddedToCookie(
    const GoogleServiceAuthError& error) {
  if (first_account_added_to_cookie_)
    return;

  first_account_added_to_cookie_ = true;

  // Regardless of whether the account was successfully added or not,
  // continue with sync starting.

  // The sync confirmation dialog should always be shown regardless of
  // |start_mode_|. |sync_setup_completed_callback_| will be run after the
  // modal is closed.
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

ProfileSyncService* OneClickSigninSyncStarter::GetProfileSyncService() {
  ProfileSyncService* service = nullptr;
  if (profile_->IsSyncAllowed())
    service = ProfileSyncServiceFactory::GetForProfile(profile_);
  return service;
}

void OneClickSigninSyncStarter::FinishProfileSyncServiceSetup() {
  sync_blocker_.reset();
}

