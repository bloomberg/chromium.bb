// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/sync/one_click_signin_sync_starter.h"

#include "base/prefs/pref_service.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"

#if defined(ENABLE_CONFIGURATION_POLICY)
#include "chrome/browser/policy/cloud/user_policy_signin_service.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service_factory.h"
#endif

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/sync_prefs.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/browser/ui/webui/signin/profile_signin_confirmation_dialog.h"
#include "chrome/common/url_constants.h"

OneClickSigninSyncStarter::OneClickSigninSyncStarter(
    Profile* profile,
    Browser* browser,
    const std::string& session_index,
    const std::string& email,
    const std::string& password,
    StartSyncMode start_mode,
    bool force_same_tab_navigation,
    bool confirmation_required)
    : start_mode_(start_mode),
      force_same_tab_navigation_(force_same_tab_navigation),
      confirmation_required_(confirmation_required),
      weak_pointer_factory_(this) {
  DCHECK(profile);
  Initialize(profile, browser);

  // Start the signin process using the cookies in the cookie jar.
  SigninManager* manager = SigninManagerFactory::GetForProfile(profile_);
  SigninManager::OAuthTokenFetchedCallback callback;
  // Policy is enabled, so pass in a callback to do extra policy-related UI
  // before signin completes.
  callback = base::Bind(&OneClickSigninSyncStarter::ConfirmSignin,
                        weak_pointer_factory_.GetWeakPtr());
  manager->StartSignInWithCredentials(session_index, email, password, callback);
}

OneClickSigninSyncStarter::~OneClickSigninSyncStarter() {
}

void OneClickSigninSyncStarter::Initialize(Profile* profile, Browser* browser) {
  DCHECK(profile);
  profile_ = profile;
  browser_ = browser;

  // Cache the parent desktop for the browser, so we can reuse that same
  // desktop for any UI we want to display.
  if (browser)
    desktop_type_ = browser->host_desktop_type();

  signin_tracker_.reset(new SigninTracker(profile_, this));

  // Let the sync service know that setup is in progress so it doesn't start
  // syncing until the user has finished any configuration.
  ProfileSyncService* profile_sync_service = GetProfileSyncService();
  if (profile_sync_service)
    profile_sync_service->SetSetupInProgress(true);

  // Make sure the syncing is not suppressed, otherwise the SigninManager
  // will not be able to complete sucessfully.
  browser_sync::SyncPrefs sync_prefs(profile_->GetPrefs());
  sync_prefs.SetStartSuppressed(false);
}

void OneClickSigninSyncStarter::GaiaCredentialsValid() {
}

void OneClickSigninSyncStarter::ConfirmSignin(const std::string& oauth_token) {
  DCHECK(!oauth_token.empty());
  SigninManager* signin = SigninManagerFactory::GetForProfile(profile_);
  // If this is a new signin (no authenticated username yet) try loading
  // policy for this user now, before any signed in services are initialized.
  // This callback is only invoked for the web-based signin flow - for the old
  // ClientLogin flow, policy will get loaded once the TokenService finishes
  // initializing (not ideal, but it's a reasonable fallback).
  if (signin->GetAuthenticatedUsername().empty()) {
#if defined(ENABLE_CONFIGURATION_POLICY)
    policy::UserPolicySigninService* policy_service =
        policy::UserPolicySigninServiceFactory::GetForProfile(profile_);
    policy_service->RegisterPolicyClient(
        signin->GetUsernameForAuthInProgress(),
        oauth_token,
        base::Bind(&OneClickSigninSyncStarter::OnRegisteredForPolicy,
                   weak_pointer_factory_.GetWeakPtr()));
    return;
#else
    SigninAfterSAMLConfirmation();
#endif
  } else {
    // The user is already signed in - just tell SigninManager to continue
    // with its re-auth flow.
    signin->CompletePendingSignin();
  }
}

#if defined(ENABLE_CONFIGURATION_POLICY)
void OneClickSigninSyncStarter::OnRegisteredForPolicy(
    scoped_ptr<policy::CloudPolicyClient> client) {
  SigninManager* signin = SigninManagerFactory::GetForProfile(profile_);
  // If there's no token for the user (policy registration did not succeed) just
  // finish signing in.
  if (!client.get()) {
    DVLOG(1) << "Policy registration failed";
    SigninAfterSAMLConfirmation();
    return;
  }

  DCHECK(client->is_registered());
  DVLOG(1) << "Policy registration succeeded: dm_token=" << client->dm_token();

  // Stash away a copy of our CloudPolicyClient (should not already have one).
  DCHECK(!policy_client_);
  policy_client_.swap(client);

  // Allow user to create a new profile before continuing with sign-in.
  ProfileSigninConfirmationDialog::ShowDialog(
      profile_,
      signin->GetUsernameForAuthInProgress(),
      base::Bind(&OneClickSigninSyncStarter::CancelSigninAndDelete,
                 weak_pointer_factory_.GetWeakPtr()),
      base::Bind(&OneClickSigninSyncStarter::CreateNewSignedInProfile,
                 weak_pointer_factory_.GetWeakPtr()),
      base::Bind(&OneClickSigninSyncStarter::LoadPolicyWithCachedClient,
                 weak_pointer_factory_.GetWeakPtr()));
}

void OneClickSigninSyncStarter::CancelSigninAndDelete() {
  SigninManagerFactory::GetForProfile(profile_)->SignOut();
  // The statement above results in a call to SigninFailed() which will free
  // this object, so do not refer to the OneClickSigninSyncStarter object
  // after this point.
}

void OneClickSigninSyncStarter::LoadPolicyWithCachedClient() {
  DCHECK(policy_client_);
  policy::UserPolicySigninService* policy_service =
      policy::UserPolicySigninServiceFactory::GetForProfile(profile_);
  policy_service->FetchPolicyForSignedInUser(
      policy_client_.Pass(),
      base::Bind(&OneClickSigninSyncStarter::OnPolicyFetchComplete,
                 weak_pointer_factory_.GetWeakPtr()));
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
  DCHECK(policy_client_);
  // Create a new profile and have it call back when done so we can inject our
  // signin credentials.
  size_t icon_index = g_browser_process->profile_manager()->
      GetProfileInfoCache().ChooseAvatarIconIndexForNewProfile();
  ProfileManager::CreateMultiProfileAsync(
      UTF8ToUTF16(signin->GetUsernameForAuthInProgress()),
      UTF8ToUTF16(ProfileInfoCache::GetDefaultAvatarIconUrl(icon_index)),
      base::Bind(&OneClickSigninSyncStarter::CompleteSigninForNewProfile,
                 weak_pointer_factory_.GetWeakPtr()),
      desktop_type_,
      false);
}

void OneClickSigninSyncStarter::CompleteSigninForNewProfile(
    Profile* new_profile,
    Profile::CreateStatus status) {
  DCHECK_NE(profile_, new_profile);
  if (status == Profile::CREATE_STATUS_FAIL) {
    // TODO(atwilson): On error, unregister the client to release the DMToken
    // and surface a better error for the user.
    NOTREACHED() << "Error creating new profile";
    CancelSigninAndDelete();
    return;
  }

  // Wait until the profile is initialized before we transfer credentials.
  if (status == Profile::CREATE_STATUS_INITIALIZED) {
    SigninManager* old_signin_manager =
        SigninManagerFactory::GetForProfile(profile_);
    SigninManager* new_signin_manager =
        SigninManagerFactory::GetForProfile(new_profile);
    DCHECK(!old_signin_manager->GetUsernameForAuthInProgress().empty());
    DCHECK(old_signin_manager->GetAuthenticatedUsername().empty());
    DCHECK(new_signin_manager->GetAuthenticatedUsername().empty());
    DCHECK(policy_client_);

    // Copy credentials from the old profile to the just-created profile,
    // and switch over to tracking that profile.
    new_signin_manager->CopyCredentialsFrom(*old_signin_manager);
    ProfileSyncService* profile_sync_service = GetProfileSyncService();
    if (profile_sync_service)
      profile_sync_service->SetSetupInProgress(false);
    Initialize(new_profile, NULL);
    DCHECK_EQ(profile_, new_profile);

    // We've transferred our credentials to the new profile - notify that
    // the signin for the original profile was cancelled (must do this after
    // we have called Initialize() with the new profile, as otherwise this
    // object will get freed when the signin on the old profile is cancelled.
    old_signin_manager->SignOut();

    // Load policy for the just-created profile - once policy has finished
    // loading the signin process will complete.
    LoadPolicyWithCachedClient();
  }
}
#endif

void OneClickSigninSyncStarter::SigninAfterSAMLConfirmation() {
  SigninManager* signin = SigninManagerFactory::GetForProfile(profile_);
  // browser_ can be null for unit tests.
  if (!browser_ || !confirmation_required_) {
    // No confirmation required - just sign in the user.
    signin->CompletePendingSignin();
  } else {
    // Display a confirmation dialog to the user.
    browser_->window()->ShowOneClickSigninBubble(
        BrowserWindow::ONE_CLICK_SIGNIN_BUBBLE_TYPE_SAML_MODAL_DIALOG,
        UTF8ToUTF16(signin->GetUsernameForAuthInProgress()),
        string16(), // No error message to display.
        base::Bind(&OneClickSigninSyncStarter::SigninConfirmationComplete,
                   weak_pointer_factory_.GetWeakPtr()));
  }
}

void OneClickSigninSyncStarter::SigninConfirmationComplete(
    StartSyncMode response) {
  if (response == UNDO_SYNC) {
    CancelSigninAndDelete();
  } else {
    // If the user clicked the "Advanced" link in the confirmation dialog, then
    // override the current start_mode_ to bring up the advanced sync settings.
    if (response == CONFIGURE_SYNC_FIRST)
      start_mode_ = response;
    SigninManager* signin = SigninManagerFactory::GetForProfile(profile_);
    signin->CompletePendingSignin();
  }
}


void OneClickSigninSyncStarter::SigninFailed(
    const GoogleServiceAuthError& error) {
  ProfileSyncService* profile_sync_service = GetProfileSyncService();
  if (profile_sync_service)
    profile_sync_service->SetSetupInProgress(false);
  delete this;
}

void OneClickSigninSyncStarter::SigninSuccess() {
  ProfileSyncService* profile_sync_service = GetProfileSyncService();
  switch (start_mode_) {
    case SYNC_WITH_DEFAULT_SETTINGS:
      if (profile_sync_service) {
        // Just kick off the sync machine, no need to configure it first.
        profile_sync_service->OnUserChoseDatatypes(true,
                                                   syncer::ModelTypeSet());
        profile_sync_service->SetSyncSetupCompleted();
        profile_sync_service->SetSetupInProgress(false);
      }
      break;
    case CONFIGURE_SYNC_FIRST:
      ConfigureSync();
      break;
    default:
      NOTREACHED() << "Invalid start_mode=" << start_mode_;
  }

  delete this;
}

void OneClickSigninSyncStarter::ConfigureSync() {
  // Give the user a chance to configure things. We don't clear the
  // ProfileSyncService::setup_in_progress flag because we don't want sync
  // to start up until after the configure UI is displayed (the configure UI
  // will clear the flag when the user is done setting up sync).
  ProfileSyncService* profile_sync_service = GetProfileSyncService();
  LoginUIService* login_ui = LoginUIServiceFactory::GetForProfile(profile_);
  if (login_ui->current_login_ui()) {
    login_ui->current_login_ui()->FocusUI();
  } else {
    if (!browser_) {
      // The user just created a new profile so we need to figure out what
      // browser to use to display settings. Grab the most recently active
      // browser or else create a new one.
      browser_ = chrome::FindLastActiveWithProfile(profile_, desktop_type_);
      if (!browser_) {
        browser_ = new Browser(Browser::CreateParams(profile_,
                                                     desktop_type_));
      }
      browser_->window()->Show();
    }
    if (profile_sync_service) {
      // Need to navigate to the settings page and display the sync UI.
      if (force_same_tab_navigation_) {
        ShowSyncSettingsPageOnSameTab();
      } else {
        chrome::ShowSettingsSubPage(browser_, chrome::kSyncSetupSubPage);
      }
    } else {
      // Sync is disabled - just display the settings page.
      chrome::ShowSettings(browser_);
    }
  }
}

ProfileSyncService* OneClickSigninSyncStarter::GetProfileSyncService() {
  ProfileSyncService* service = NULL;
  if (profile_->IsSyncAccessible())
    service = ProfileSyncServiceFactory::GetForProfile(profile_);
  return service;
}

void OneClickSigninSyncStarter::ShowSyncSettingsPageOnSameTab() {
  std::string url = std::string(chrome::kChromeUISettingsURL) +
      chrome::kSyncSetupSubPage;
  chrome::NavigateParams params(
      browser_, GURL(url), content::PAGE_TRANSITION_AUTO_TOPLEVEL);
  params.disposition = CURRENT_TAB;
  params.window_action = chrome::NavigateParams::SHOW_WINDOW;
  chrome::Navigate(&params);
}
