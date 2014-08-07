// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/sync/one_click_signin_sync_starter.h"

#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"

#if defined(ENABLE_CONFIGURATION_POLICY)
#include "chrome/browser/policy/cloud/user_policy_signin_service.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service_factory.h"
#endif

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/signin/signin_tracker_factory.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/sync/one_click_signin_sync_observer.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/browser/ui/webui/signin/profile_signin_confirmation_dialog.h"
#include "chrome/common/url_constants.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/signin/core/browser/signin_metrics.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "components/sync_driver/sync_prefs.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "net/url_request/url_request_context_getter.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

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
    const std::string& email,
    const std::string& password,
    const std::string& refresh_token,
    StartSyncMode start_mode,
    content::WebContents* web_contents,
    ConfirmationRequired confirmation_required,
    const GURL& continue_url,
    Callback sync_setup_completed_callback)
    : content::WebContentsObserver(web_contents),
      start_mode_(start_mode),
      desktop_type_(chrome::HOST_DESKTOP_TYPE_NATIVE),
      confirmation_required_(confirmation_required),
      continue_url_(continue_url),
      sync_setup_completed_callback_(sync_setup_completed_callback),
      weak_pointer_factory_(this) {
  DCHECK(profile);
  DCHECK(web_contents || continue_url.is_empty());
  BrowserList::AddObserver(this);
  LoginUIServiceFactory::GetForProfile(profile)->AddObserver(this);
  Initialize(profile, browser);

  // Policy is enabled, so pass in a callback to do extra policy-related UI
  // before signin completes.
  SigninManagerFactory::GetForProfile(profile_)->
      StartSignInWithRefreshToken(
          refresh_token, email, password,
          base::Bind(&OneClickSigninSyncStarter::ConfirmSignin,
                     weak_pointer_factory_.GetWeakPtr()));
}

void OneClickSigninSyncStarter::OnBrowserRemoved(Browser* browser) {
  if (browser == browser_)
    browser_ = NULL;
}

OneClickSigninSyncStarter::~OneClickSigninSyncStarter() {
  BrowserList::RemoveObserver(this);
  LoginUIServiceFactory::GetForProfile(profile_)->RemoveObserver(this);
}

void OneClickSigninSyncStarter::Initialize(Profile* profile, Browser* browser) {
  DCHECK(profile);
  profile_ = profile;
  browser_ = browser;

  // Cache the parent desktop for the browser, so we can reuse that same
  // desktop for any UI we want to display.
  if (browser) {
    desktop_type_ = browser->host_desktop_type();
  } else {
    desktop_type_ = chrome::GetActiveDesktop();
  }

  signin_tracker_ = SigninTrackerFactory::CreateForProfile(profile_, this);

  // Let the sync service know that setup is in progress so it doesn't start
  // syncing until the user has finished any configuration.
  ProfileSyncService* profile_sync_service = GetProfileSyncService();
  if (profile_sync_service)
    profile_sync_service->SetSetupInProgress(true);

  // Make sure the syncing is not suppressed, otherwise the SigninManager
  // will not be able to complete sucessfully.
  sync_driver::SyncPrefs sync_prefs(profile_->GetPrefs());
  sync_prefs.SetStartSuppressed(false);
}

void OneClickSigninSyncStarter::ConfirmSignin(const std::string& oauth_token) {
  DCHECK(!oauth_token.empty());
  SigninManager* signin = SigninManagerFactory::GetForProfile(profile_);
  // If this is a new signin (no authenticated username yet) try loading
  // policy for this user now, before any signed in services are initialized.
  if (signin->GetAuthenticatedUsername().empty()) {
#if defined(ENABLE_CONFIGURATION_POLICY)
    policy::UserPolicySigninService* policy_service =
        policy::UserPolicySigninServiceFactory::GetForProfile(profile_);
    policy_service->RegisterForPolicy(
        signin->GetUsernameForAuthInProgress(),
        oauth_token,
        base::Bind(&OneClickSigninSyncStarter::OnRegisteredForPolicy,
                   weak_pointer_factory_.GetWeakPtr()));
    return;
#else
    ConfirmAndSignin();
#endif
  } else {
    // The user is already signed in - just tell SigninManager to continue
    // with its re-auth flow.
    signin->CompletePendingSignin();
  }
}

#if defined(ENABLE_CONFIGURATION_POLICY)
OneClickSigninSyncStarter::SigninDialogDelegate::SigninDialogDelegate(
    base::WeakPtr<OneClickSigninSyncStarter> sync_starter)
  : sync_starter_(sync_starter) {
}

OneClickSigninSyncStarter::SigninDialogDelegate::~SigninDialogDelegate() {
}

void OneClickSigninSyncStarter::SigninDialogDelegate::OnCancelSignin() {
  SetUserChoiceHistogram(SIGNIN_CHOICE_CANCEL);
  if (sync_starter_ != NULL)
    sync_starter_->CancelSigninAndDelete();
}

void OneClickSigninSyncStarter::SigninDialogDelegate::OnContinueSignin() {
  SetUserChoiceHistogram(SIGNIN_CHOICE_CONTINUE);

  if (sync_starter_ != NULL) {
    // If the user signs in from the new avatar bubble, the enterprise
    // confirmation dialog would dismiss the avatar bubble, thus it won't show
    // any confirmation upon sign in completes. This cofirmation dialog already
    // mentions that user data would be synced, thus we just start sync
    // immediately.

    // TODO(guohui): add a sync settings link to allow user to configure sync
    // settings before sync starts.
    if (sync_starter_->GetStartSyncMode() == CONFIRM_SYNC_SETTINGS_FIRST)
      sync_starter_->SetStartSyncMode(SYNC_WITH_DEFAULT_SETTINGS);
    sync_starter_->LoadPolicyWithCachedCredentials();
  }
}

void OneClickSigninSyncStarter::SigninDialogDelegate::OnSigninWithNewProfile() {
  SetUserChoiceHistogram(SIGNIN_CHOICE_NEW_PROFILE);

  if (sync_starter_ != NULL) {
    // TODO(guohui): add a sync settings link to allow user to configure sync
    // settings before sync starts.
    if (sync_starter_->GetStartSyncMode() == CONFIRM_SYNC_SETTINGS_FIRST)
      sync_starter_->SetStartSyncMode(SYNC_WITH_DEFAULT_SETTINGS);
    sync_starter_->CreateNewSignedInProfile();
  }
}

void OneClickSigninSyncStarter::SetStartSyncMode(StartSyncMode start_mode) {
  start_mode_ = start_mode;
}

OneClickSigninSyncStarter::StartSyncMode
    OneClickSigninSyncStarter::GetStartSyncMode() {
  return start_mode_;
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
  browser_ = EnsureBrowser(browser_, profile_, desktop_type_);
  content::WebContents* web_contents =
      browser_->tab_strip_model()->GetActiveWebContents();
  if (!web_contents) {
    CancelSigninAndDelete();
    return;
  }
  chrome::ShowProfileSigninConfirmationDialog(
      browser_,
      web_contents,
      profile_,
      signin->GetUsernameForAuthInProgress(),
      new SigninDialogDelegate(weak_pointer_factory_.GetWeakPtr()));
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
  DCHECK(!dm_token_.empty());
  DCHECK(!client_id_.empty());
  // Create a new profile and have it call back when done so we can inject our
  // signin credentials.
  size_t icon_index = g_browser_process->profile_manager()->
      GetProfileInfoCache().ChooseAvatarIconIndexForNewProfile();
  ProfileManager::CreateMultiProfileAsync(
      base::UTF8ToUTF16(signin->GetUsernameForAuthInProgress()),
      base::UTF8ToUTF16(profiles::GetDefaultAvatarIconUrl(icon_index)),
      base::Bind(&OneClickSigninSyncStarter::CompleteInitForNewProfile,
                 weak_pointer_factory_.GetWeakPtr(), desktop_type_),
      std::string());
}

void OneClickSigninSyncStarter::CompleteInitForNewProfile(
    chrome::HostDesktopType desktop_type,
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
      DCHECK(old_signin_manager->GetAuthenticatedUsername().empty());
      DCHECK(new_signin_manager->GetAuthenticatedUsername().empty());
      DCHECK(!dm_token_.empty());
      DCHECK(!client_id_.empty());

      // Copy credentials from the old profile to the just-created profile,
      // and switch over to tracking that profile.
      new_signin_manager->CopyCredentialsFrom(*old_signin_manager);
      FinishProfileSyncServiceSetup();
      Initialize(new_profile, NULL);
      DCHECK_EQ(profile_, new_profile);

      // We've transferred our credentials to the new profile - notify that
      // the signin for the original profile was cancelled (must do this after
      // we have called Initialize() with the new profile, as otherwise this
      // object will get freed when the signin on the old profile is cancelled.
      old_signin_manager->SignOut(signin_metrics::TRANSFER_CREDENTIALS);

      // Load policy for the just-created profile - once policy has finished
      // loading the signin process will complete.
      LoadPolicyWithCachedCredentials();

      // Open the profile's first window, after all initialization.
      profiles::FindOrCreateNewWindowForProfile(
        new_profile,
        chrome::startup::IS_PROCESS_STARTUP,
        chrome::startup::IS_FIRST_RUN,
        desktop_type,
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
#endif

void OneClickSigninSyncStarter::CancelSigninAndDelete() {
  SigninManagerFactory::GetForProfile(profile_)->SignOut(
      signin_metrics::ABORT_SIGNIN);
  // The statement above results in a call to SigninFailed() which will free
  // this object, so do not refer to the OneClickSigninSyncStarter object
  // after this point.
}

void OneClickSigninSyncStarter::ConfirmAndSignin() {
  SigninManager* signin = SigninManagerFactory::GetForProfile(profile_);
  if (confirmation_required_ == CONFIRM_UNTRUSTED_SIGNIN) {
    browser_ = EnsureBrowser(browser_, profile_, desktop_type_);
    // Display a confirmation dialog to the user.
    browser_->window()->ShowOneClickSigninBubble(
        BrowserWindow::ONE_CLICK_SIGNIN_BUBBLE_TYPE_SAML_MODAL_DIALOG,
        base::UTF8ToUTF16(signin->GetUsernameForAuthInProgress()),
        base::string16(),  // No error message to display.
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
    bool configure_sync_first) {
  if (configure_sync_first) {
    chrome::ShowSettingsSubPage(browser_, chrome::kSyncSetupSubPage);
  } else {
    ProfileSyncService* profile_sync_service = GetProfileSyncService();
    if (profile_sync_service)
      profile_sync_service->SetSyncSetupCompleted();
    FinishProfileSyncServiceSetup();
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
  if (switches::IsEnableWebBasedSignin())
    MergeSessionComplete(GoogleServiceAuthError(GoogleServiceAuthError::NONE));
}

void OneClickSigninSyncStarter::MergeSessionComplete(
    const GoogleServiceAuthError& error) {
  // Regardless of whether the merge session completed sucessfully or not,
  // continue with sync starting.

  if (!sync_setup_completed_callback_.is_null())
    sync_setup_completed_callback_.Run(SYNC_SETUP_SUCCESS);

  switch (start_mode_) {
    case SYNC_WITH_DEFAULT_SETTINGS: {
      // Just kick off the sync machine, no need to configure it first.
      ProfileSyncService* profile_sync_service = GetProfileSyncService();
      if (profile_sync_service)
        profile_sync_service->SetSyncSetupCompleted();
      FinishProfileSyncServiceSetup();
      if (confirmation_required_ == CONFIRM_AFTER_SIGNIN) {
        base::string16 message;
        if (!profile_sync_service) {
          // Sync is disabled by policy.
          message = l10n_util::GetStringUTF16(
              IDS_ONE_CLICK_SIGNIN_BUBBLE_SYNC_DISABLED_MESSAGE);
        }
        DisplayFinalConfirmationBubble(message);
      }
      break;
    }
    case CONFIRM_SYNC_SETTINGS_FIRST:
      // Blocks sync until the sync settings confirmation UI is closed.
      return;
    case CONFIGURE_SYNC_FIRST:
      ShowSettingsPage(true);  // Show sync config UI.
      break;
    case SHOW_SETTINGS_WITHOUT_CONFIGURE:
      ShowSettingsPage(false);  // Don't show sync config UI.
      break;
    case UNDO_SYNC:
      NOTREACHED();
  }

  // Navigate to the |continue_url_| if one is set, unless the user first needs
  // to configure Sync.
  if (web_contents() && !continue_url_.is_empty() &&
      start_mode_ != CONFIGURE_SYNC_FIRST) {
    LoadContinueUrl();
  }

  delete this;
}

void OneClickSigninSyncStarter::DisplayFinalConfirmationBubble(
    const base::string16& custom_message) {
  browser_ = EnsureBrowser(browser_, profile_, desktop_type_);
  browser_->window()->ShowOneClickSigninBubble(
      BrowserWindow::ONE_CLICK_SIGNIN_BUBBLE_TYPE_BUBBLE,
      base::string16(),  // No email required - this is not a SAML confirmation.
      custom_message,
      // Callback is ignored.
      BrowserWindow::StartSyncCallback());
}

// static
Browser* OneClickSigninSyncStarter::EnsureBrowser(
    Browser* browser,
    Profile* profile,
    chrome::HostDesktopType desktop_type) {
  if (!browser) {
    // The user just created a new profile or has closed the browser that
    // we used previously. Grab the most recently active browser or else
    // create a new one.
    browser = chrome::FindLastActiveWithProfile(profile, desktop_type);
    if (!browser) {
      browser = new Browser(Browser::CreateParams(profile,
                                                   desktop_type));
      chrome::AddTabAt(browser, GURL(), -1, true);
    }
    browser->window()->Show();
  }
  return browser;
}

void OneClickSigninSyncStarter::ShowSettingsPage(bool configure_sync) {
  // Give the user a chance to configure things. We don't clear the
  // ProfileSyncService::setup_in_progress flag because we don't want sync
  // to start up until after the configure UI is displayed (the configure UI
  // will clear the flag when the user is done setting up sync).
  ProfileSyncService* profile_sync_service = GetProfileSyncService();
  LoginUIService* login_ui = LoginUIServiceFactory::GetForProfile(profile_);
  if (login_ui->current_login_ui()) {
    login_ui->current_login_ui()->FocusUI();
  } else {
    browser_ = EnsureBrowser(browser_, profile_, desktop_type_);

    // If the sign in tab is showing the native signin page or the blank page
    // for web-based flow, and is not about to be closed, use it to show the
    // settings UI.
    bool use_same_tab = false;
    if (web_contents()) {
      GURL current_url = web_contents()->GetLastCommittedURL();
      bool is_chrome_signin_url =
          current_url.GetOrigin().spec() == chrome::kChromeUIChromeSigninURL;
      bool is_same_profile =
          Profile::FromBrowserContext(web_contents()->GetBrowserContext()) ==
          profile_;
      use_same_tab =
          (is_chrome_signin_url ||
           signin::IsContinueUrlForWebBasedSigninFlow(current_url)) &&
          !signin::IsAutoCloseEnabledInURL(current_url) &&
          is_same_profile;
    }
    if (profile_sync_service) {
      // Need to navigate to the settings page and display the sync UI.
      if (use_same_tab) {
        ShowSettingsPageInWebContents(web_contents(),
                                      chrome::kSyncSetupSubPage);
      } else {
        // If the user is setting up sync for the first time, let them configure
        // advanced sync settings. However, in the case of re-authentication,
        // return the user to the settings page without showing any config UI.
        if (configure_sync) {
          chrome::ShowSettingsSubPage(browser_, chrome::kSyncSetupSubPage);
        } else {
          FinishProfileSyncServiceSetup();
          chrome::ShowSettings(browser_);
        }
      }
    } else {
      // Sync is disabled - just display the settings page or redirect to the
      // |continue_url_|.
      FinishProfileSyncServiceSetup();
      if (!use_same_tab)
        chrome::ShowSettings(browser_);
      else if (!continue_url_.is_empty())
        LoadContinueUrl();
      else
        ShowSettingsPageInWebContents(web_contents(), std::string());
    }
  }
}

ProfileSyncService* OneClickSigninSyncStarter::GetProfileSyncService() {
  ProfileSyncService* service = NULL;
  if (profile_->IsSyncAccessible())
    service = ProfileSyncServiceFactory::GetForProfile(profile_);
  return service;
}

void OneClickSigninSyncStarter::FinishProfileSyncServiceSetup() {
  ProfileSyncService* service =
      ProfileSyncServiceFactory::GetForProfile(profile_);
  if (service)
    service->SetSetupInProgress(false);
}

void OneClickSigninSyncStarter::ShowSettingsPageInWebContents(
    content::WebContents* contents,
    const std::string& sub_page) {
  if (!continue_url_.is_empty()) {
    // The observer deletes itself once it's done.
    DCHECK(!sub_page.empty());
    new OneClickSigninSyncObserver(contents, continue_url_);
  }

  GURL url = chrome::GetSettingsUrl(sub_page);
  content::OpenURLParams params(url,
                                content::Referrer(),
                                CURRENT_TAB,
                                content::PAGE_TRANSITION_AUTO_TOPLEVEL,
                                false);
  contents->OpenURL(params);

  // Activate the tab.
  Browser* browser = chrome::FindBrowserWithWebContents(contents);
  int content_index =
      browser->tab_strip_model()->GetIndexOfWebContents(contents);
  browser->tab_strip_model()->ActivateTabAt(content_index,
                                            false /* user_gesture */);
}

void OneClickSigninSyncStarter::LoadContinueUrl() {
  web_contents()->GetController().LoadURL(
      continue_url_,
      content::Referrer(),
      content::PAGE_TRANSITION_AUTO_TOPLEVEL,
      std::string());
}
