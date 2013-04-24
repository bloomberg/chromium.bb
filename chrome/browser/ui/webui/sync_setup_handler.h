// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SYNC_SETUP_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SYNC_SETUP_HANDLER_H_

#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/timer.h"
#include "chrome/browser/signin/signin_tracker.h"
#include "chrome/browser/ui/webui/options/options_ui.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "content/public/browser/web_contents_observer.h"

class LoginUIService;
class ProfileManager;
class ProfileSyncService;
class SigninManagerBase;

namespace content {
class WebContents;
}

class SyncSetupHandler : public options::OptionsPageUIHandler,
                         public SigninTracker::Observer,
                         public LoginUIService::LoginUI,
                         public content::WebContentsObserver {
 public:
  // Constructs a new SyncSetupHandler. |profile_manager| may be NULL.
  explicit SyncSetupHandler(ProfileManager* profile_manager);
  virtual ~SyncSetupHandler();

  // OptionsPageUIHandler implementation.
  virtual void GetLocalizedValues(base::DictionaryValue* localized_strings)
      OVERRIDE;
  virtual void RegisterMessages() OVERRIDE;

  // SigninTracker::Observer implementation.
  virtual void GaiaCredentialsValid() OVERRIDE;
  virtual void SigninFailed(const GoogleServiceAuthError& error) OVERRIDE;
  virtual void SigninSuccess() OVERRIDE;

  // LoginUIService::LoginUI implementation.
  virtual void FocusUI() OVERRIDE;
  virtual void CloseUI() OVERRIDE;

  // content::WebContentsObserver implementation.
  virtual void DidStopLoading(
      content::RenderViewHost* render_view_host) OVERRIDE;
  virtual void WebContentsDestroyed(
      content::WebContents* web_contents) OVERRIDE;

  static void GetStaticLocalizedValues(
      base::DictionaryValue* localized_strings,
      content::WebUI* web_ui);

  // Initializes the sync setup flow and shows the setup UI.
  void OpenSyncSetup();

  // Shows advanced configuration dialog without going through sign in dialog.
  // Kicks the sync backend if necessary with showing spinner dialog until it
  // gets ready.
  void OpenConfigureSync();

  // Terminates the sync setup flow.
  void CloseSyncSetup();

 protected:
  FRIEND_TEST_ALL_PREFIXES(SyncSetupHandlerTest, DisplayBasicLogin);
  FRIEND_TEST_ALL_PREFIXES(SyncSetupHandlerTest,
                           DisplayConfigureWithBackendDisabledAndCancel);
  FRIEND_TEST_ALL_PREFIXES(SyncSetupHandlerTest, SelectCustomEncryption);
  FRIEND_TEST_ALL_PREFIXES(SyncSetupHandlerTest, ShowSyncSetupWhenNotSignedIn);
  FRIEND_TEST_ALL_PREFIXES(SyncSetupHandlerTest, SuccessfullySetPassphrase);
  FRIEND_TEST_ALL_PREFIXES(SyncSetupHandlerTest, TestSyncEverything);
  FRIEND_TEST_ALL_PREFIXES(SyncSetupHandlerTest, TestSyncAllManually);
  FRIEND_TEST_ALL_PREFIXES(SyncSetupHandlerTest, TestPassphraseStillRequired);
  FRIEND_TEST_ALL_PREFIXES(SyncSetupHandlerTest, TestSyncIndividualTypes);
  FRIEND_TEST_ALL_PREFIXES(SyncSetupHandlerTest, TurnOnEncryptAll);
  FRIEND_TEST_ALL_PREFIXES(SyncSetupHandlerTest, UnsuccessfullySetPassphrase);
  FRIEND_TEST_ALL_PREFIXES(SyncSetupHandlerNonCrosTest,
                           UnrecoverableErrorInitializingSync);
  FRIEND_TEST_ALL_PREFIXES(SyncSetupHandlerNonCrosTest,
                           GaiaErrorInitializingSync);
  FRIEND_TEST_ALL_PREFIXES(SyncSetupHandlerNonCrosTest, HandleCaptcha);
  FRIEND_TEST_ALL_PREFIXES(SyncSetupHandlerNonCrosTest, HandleGaiaAuthFailure);
  FRIEND_TEST_ALL_PREFIXES(SyncSetupHandlerNonCrosTest,
                           SubmitAuthWithInvalidUsername);

  bool is_configuring_sync() const { return configuring_sync_; }
  bool have_signin_tracker() const { return signin_tracker_; }

  // Subclasses must implement this to show the setup UI that's appropriate
  // for the page this is contained in.
  virtual void ShowSetupUI() = 0;

  // Overridden by subclasses (like SyncPromoHandler) to log stats about the
  // user's signin activity.
  virtual void RecordSignin();

  // Display the configure sync UI. If |show_advanced| is true, skip directly
  // to the "advanced settings" dialog, otherwise give the user the simpler
  // "Sync Everything" dialog. Overridden by subclasses to allow them to skip
  // the sync setup dialog if desired.
  // If |passphrase_failed| is true, then the user previously tried to enter an
  // invalid passphrase.
  virtual void DisplayConfigureSync(bool show_advanced, bool passphrase_failed);

  // Called when configuring sync is done to close the dialog and start syncing.
  void ConfigureSyncDone();

  // Helper routine that gets the ProfileSyncService associated with the parent
  // profile.
  ProfileSyncService* GetSyncService() const;

  // Returns the LoginUIService for the parent profile.
  LoginUIService* GetLoginUIService() const;

 private:
  // Callbacks from the page.
  void OnDidClosePage(const base::ListValue* args);
  void HandleConfigure(const base::ListValue* args);
  void HandlePassphraseEntry(const base::ListValue* args);
  void HandlePassphraseCancel(const base::ListValue* args);
  void HandleShowErrorUI(const base::ListValue* args);
  void HandleShowSetupUI(const base::ListValue* args);
  void HandleShowSetupUIWithoutLogin(const base::ListValue* args);
  void HandleDoSignOutOnAuthError(const base::ListValue* args);
  void HandleStartSignin(const base::ListValue* args);
  void HandleStopSyncing(const base::ListValue* args);
  void HandleCloseTimeout(const base::ListValue* args);
#if !defined(OS_CHROMEOS)
  void HandleSubmitAuth(const base::ListValue* args);

  // Initiates a login via the signin manager.
  void TryLogin(const std::string& username,
                const std::string& password,
                const std::string& captcha,
                const std::string& access_code);
#endif

  // Helper routine that gets the Profile associated with this object (virtual
  // so tests can override).
  virtual Profile* GetProfile() const;

  // Shows the GAIA login success page then exits.
  void DisplayGaiaSuccessAndClose();

  // Displays the GAIA login success page then transitions to sync setup.
  void DisplayGaiaSuccessAndSettingUp();

  // Displays the GAIA login form. If |fatal_error| is true, displays the fatal
  // error UI.
  void DisplayGaiaLogin(bool fatal_error);

  // When web-flow is enabled, displays the Gaia login form in a new tab.
  // This function is virtual so that tests can override.
  virtual void DisplayGaiaLoginInNewTabOrWindow();

  // Displays the GAIA login form with a custom error message (used for errors
  // like "email address already in use by another profile"). No message
  // displayed if |error_message| is empty. Displays fatal error UI if
  // |fatal_error| = true.
  void DisplayGaiaLoginWithErrorMessage(const string16& error_message,
                                        bool fatal_error);

  // A utility function to call before actually showing setup dialog. Makes sure
  // that a new dialog can be shown and sets flag that setup is in progress.
  bool PrepareSyncSetup();

  // Displays spinner-only UI indicating that something is going on in the
  // background.
  // TODO(kochi): better to show some message that the user can understand what
  // is running in the background.
  void DisplaySpinner();

  // Displays an error dialog which shows timeout of starting the sync backend.
  void DisplayTimeout();

  // Returns true if this object is the active login object.
  bool IsActiveLogin() const;

  // If a wizard already exists, focus it and return true.
  bool FocusExistingWizardIfPresent();

  // Invokes the javascript call to close the setup overlay.
  void CloseOverlay();

  // When using web-flow, closes the Gaia page used to collection user
  // credentials.
  void CloseGaiaSigninPage();

  // Returns true if the given login data is valid, false otherwise. If the
  // login data is not valid then on return |error_message| will be set to  a
  // localized error message. Note, |error_message| must not be NULL.
  bool IsLoginAuthDataValid(const std::string& username,
                            string16* error_message);

  // The SigninTracker object used to determine when the user has fully signed
  // in (this requires waiting for various services to initialize and tracking
  // errors from multiple sources). Should only be non-null while the login UI
  // is visible.
  scoped_ptr<SigninTracker> signin_tracker_;

  // Set to true whenever the sync configure UI is visible. This is used to tell
  // what stage of the setup wizard the user was in and to update the UMA
  // histograms in the case that the user cancels out.
  bool configuring_sync_;

  // Weak reference to the profile manager.
  ProfileManager* const profile_manager_;

  // Cache of the last name the client attempted to authenticate.
  std::string last_attempted_user_email_;

  // The error from the last signin attempt.
  GoogleServiceAuthError last_signin_error_;

  // When setup starts with login UI, retry login if signing in failed.
  // When setup starts without login UI, do not retry login and fail.
  bool retry_on_signin_failure_;

  // The OneShotTimer object used to timeout of starting the sync backend
  // service.
  scoped_ptr<base::OneShotTimer<SyncSetupHandler> > backend_start_timer_;

  // When using web-flow, weak pointer to the tab that holds the Gaia sign in
  // page.
  content::WebContents* active_gaia_signin_tab_;

  DISALLOW_COPY_AND_ASSIGN(SyncSetupHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_SYNC_SETUP_HANDLER_H_
