// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_SYNC_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_SYNC_HANDLER_H_

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_change_registrar.h"
#include "base/scoped_observer.h"
#include "base/timer/timer.h"
#include "chrome/browser/sync/sync_startup_tracker.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "components/signin/core/browser/signin_manager_base.h"
#include "components/sync_driver/sync_service_observer.h"
#include "content/public/browser/web_ui_message_handler.h"

class LoginUIService;
class ProfileSyncService;
class SigninManagerBase;

namespace content {
class WebContents;
class WebUI;
}

namespace settings {

class SyncHandler : public content::WebUIMessageHandler,
                    public SigninManagerBase::Observer,
                    public SyncStartupTracker::Observer,
                    public LoginUIService::LoginUI,
                    public sync_driver::SyncServiceObserver {
 public:
  explicit SyncHandler(Profile* profile);
  ~SyncHandler() override;

  // content::WebUIMessageHandler implementation.
  void RegisterMessages() override;

  // SyncStartupTracker::Observer implementation.
  void SyncStartupCompleted() override;
  void SyncStartupFailed() override;

  // LoginUIService::LoginUI implementation.
  void FocusUI() override;
  void CloseUI() override;

  // SigninManagerBase::Observer implementation.
  void GoogleSigninSucceeded(const std::string& account_id,
                             const std::string& username,
                             const std::string& password) override;
  void GoogleSignedOut(const std::string& account_id,
                       const std::string& username) override;

  // sync_driver::SyncServiceObserver implementation.
  void OnStateChanged() override;

  // Initializes the sync setup flow and shows the setup UI.
  void OpenSyncSetup();

  // Shows advanced configuration dialog without going through sign in dialog.
  // Kicks the sync backend if necessary with showing spinner dialog until it
  // gets ready.
  void OpenConfigureSync();

  // Terminates the sync setup flow.
  void CloseSyncSetup();

  // Returns a newly created dictionary with a number of properties that
  // correspond to the status of sync.
  scoped_ptr<base::DictionaryValue> GetSyncStateDictionary();

 protected:
  friend class SyncHandlerTest;
  FRIEND_TEST_ALL_PREFIXES(SyncHandlerTest,
                           DisplayConfigureWithBackendDisabledAndCancel);
  FRIEND_TEST_ALL_PREFIXES(SyncHandlerTest, HandleSetupUIWhenSyncDisabled);
  FRIEND_TEST_ALL_PREFIXES(SyncHandlerTest, SelectCustomEncryption);
  FRIEND_TEST_ALL_PREFIXES(SyncHandlerTest, ShowSyncSetupWhenNotSignedIn);
  FRIEND_TEST_ALL_PREFIXES(SyncHandlerTest, SuccessfullySetPassphrase);
  FRIEND_TEST_ALL_PREFIXES(SyncHandlerTest, TestSyncEverything);
  FRIEND_TEST_ALL_PREFIXES(SyncHandlerTest, TestSyncNothing);
  FRIEND_TEST_ALL_PREFIXES(SyncHandlerTest, TestSyncAllManually);
  FRIEND_TEST_ALL_PREFIXES(SyncHandlerTest, TestPassphraseStillRequired);
  FRIEND_TEST_ALL_PREFIXES(SyncHandlerTest, TestSyncIndividualTypes);
  FRIEND_TEST_ALL_PREFIXES(SyncHandlerTest, TurnOnEncryptAll);
  FRIEND_TEST_ALL_PREFIXES(SyncHandlerTest, TurnOnEncryptAllDisallowed);
  FRIEND_TEST_ALL_PREFIXES(SyncHandlerTest, UnsuccessfullySetPassphrase);
  FRIEND_TEST_ALL_PREFIXES(SyncHandlerNonCrosTest,
                           UnrecoverableErrorInitializingSync);
  FRIEND_TEST_ALL_PREFIXES(SyncHandlerNonCrosTest,
                           GaiaErrorInitializingSync);
  FRIEND_TEST_ALL_PREFIXES(SyncHandlerNonCrosTest, HandleCaptcha);
  FRIEND_TEST_ALL_PREFIXES(SyncHandlerNonCrosTest, HandleGaiaAuthFailure);
  FRIEND_TEST_ALL_PREFIXES(SyncHandlerNonCrosTest,
                           SubmitAuthWithInvalidUsername);
  FRIEND_TEST_ALL_PREFIXES(SyncHandlerFirstSigninTest, DisplayBasicLogin);

  bool is_configuring_sync() const { return configuring_sync_; }

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
  void HandleShowSetupUI(const base::ListValue* args);
  void HandleDoSignOutOnAuthError(const base::ListValue* args);
  void HandleStartSignin(const base::ListValue* args);
  void HandleStopSyncing(const base::ListValue* args);
  void HandleCloseTimeout(const base::ListValue* args);
  void HandleGetSyncStatus(const base::ListValue* args);

#if !defined(OS_CHROMEOS)
  // Displays the GAIA login form.
  void DisplayGaiaLogin();

  // When web-flow is enabled, displays the Gaia login form in a new tab.
  // This function is virtual so that tests can override.
  virtual void DisplayGaiaLoginInNewTabOrWindow();
#endif

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

  // If a wizard already exists, return true. Otherwise, return false.
  bool IsExistingWizardPresent();

  // If a wizard already exists, focus it and return true.
  bool FocusExistingWizardIfPresent();

  // Display the configure sync UI. If |passphrase_failed| is true, the account
  // requires a passphrase and one hasn't been provided or it was invalid.
  void DisplayConfigureSync(bool passphrase_failed);

  // Sends the current sync status to the JavaScript WebUI code.
  void UpdateSyncState();

  // Will be called when the kSigninAllowed pref has changed.
  void OnSigninAllowedPrefChange();

  // Weak pointer.
  Profile* profile_;

  // Helper object used to wait for the sync backend to startup.
  scoped_ptr<SyncStartupTracker> sync_startup_tracker_;

  // Set to true whenever the sync configure UI is visible. This is used to tell
  // what stage of the setup wizard the user was in and to update the UMA
  // histograms in the case that the user cancels out.
  bool configuring_sync_;

  // The OneShotTimer object used to timeout of starting the sync backend
  // service.
  scoped_ptr<base::OneShotTimer> backend_start_timer_;

  // Used to listen for pref changes to allow or disallow signin.
  PrefChangeRegistrar profile_pref_registrar_;

  // Manages observer lifetime.
  ScopedObserver<ProfileSyncService, SyncHandler> sync_service_observer_;

  DISALLOW_COPY_AND_ASSIGN(SyncHandler);
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_SYNC_HANDLER_H_
