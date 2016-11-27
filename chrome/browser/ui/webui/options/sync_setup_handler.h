// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_SYNC_SETUP_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_SYNC_SETUP_HANDLER_H_

#include <memory>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "chrome/browser/sync/sync_startup_tracker.h"
#include "chrome/browser/ui/webui/options/options_ui.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"

class LoginUIService;

namespace browser_sync {
class ProfileSyncService;
}  // namespace browser_sync

namespace signin_metrics {
enum class AccessPoint;
}  // namespace signin_metrics

namespace syncer {
class SyncSetupInProgressHandle;
}  // namespace syncer

class SyncSetupHandler : public options::OptionsPageUIHandler,
                         public SyncStartupTracker::Observer,
                         public LoginUIService::LoginUI {
 public:
  SyncSetupHandler();
  ~SyncSetupHandler() override;

  // OptionsPageUIHandler implementation.
  void GetLocalizedValues(base::DictionaryValue* localized_strings) override;
  void RegisterMessages() override;

  // SyncStartupTracker::Observer implementation;
  void SyncStartupCompleted() override;
  void SyncStartupFailed() override;

  // LoginUIService::LoginUI implementation.
  void FocusUI() override;

  static void GetStaticLocalizedValues(
      base::DictionaryValue* localized_strings,
      content::WebUI* web_ui);

  // Initializes the sync setup flow and shows the setup UI.
  void OpenSyncSetup(bool creating_supervised_user);

  // Shows advanced configuration dialog without going through sign in dialog.
  // Kicks the sync backend if necessary with showing spinner dialog until it
  // gets ready.
  void OpenConfigureSync();

  // Terminates the sync setup flow.
  void CloseSyncSetup();

 protected:
  friend class SyncSetupHandlerTest;
  FRIEND_TEST_ALL_PREFIXES(SyncSetupHandlerTest,
                           DisplayConfigureWithBackendDisabledAndCancel);
  FRIEND_TEST_ALL_PREFIXES(SyncSetupHandlerTest, HandleSetupUIWhenSyncDisabled);
  FRIEND_TEST_ALL_PREFIXES(SyncSetupHandlerTest, SelectCustomEncryption);
  FRIEND_TEST_ALL_PREFIXES(SyncSetupHandlerTest, ShowSyncSetupWhenNotSignedIn);
  FRIEND_TEST_ALL_PREFIXES(SyncSetupHandlerTest, SuccessfullySetPassphrase);
  FRIEND_TEST_ALL_PREFIXES(SyncSetupHandlerTest, TestSyncEverything);
  FRIEND_TEST_ALL_PREFIXES(SyncSetupHandlerTest, TestSyncNothing);
  FRIEND_TEST_ALL_PREFIXES(SyncSetupHandlerTest, TestSyncAllManually);
  FRIEND_TEST_ALL_PREFIXES(SyncSetupHandlerTest, TestPassphraseStillRequired);
  FRIEND_TEST_ALL_PREFIXES(SyncSetupHandlerTest, TestSyncIndividualTypes);
  FRIEND_TEST_ALL_PREFIXES(SyncSetupHandlerTest, TurnOnEncryptAll);
  FRIEND_TEST_ALL_PREFIXES(SyncSetupHandlerTest, TurnOnEncryptAllDisallowed);
  FRIEND_TEST_ALL_PREFIXES(SyncSetupHandlerTest, UnsuccessfullySetPassphrase);
  FRIEND_TEST_ALL_PREFIXES(SyncSetupHandlerNonCrosTest,
                           UnrecoverableErrorInitializingSync);
  FRIEND_TEST_ALL_PREFIXES(SyncSetupHandlerNonCrosTest,
                           GaiaErrorInitializingSync);
  FRIEND_TEST_ALL_PREFIXES(SyncSetupHandlerNonCrosTest, HandleCaptcha);
  FRIEND_TEST_ALL_PREFIXES(SyncSetupHandlerNonCrosTest, HandleGaiaAuthFailure);
  FRIEND_TEST_ALL_PREFIXES(SyncSetupHandlerNonCrosTest,
                           SubmitAuthWithInvalidUsername);
  FRIEND_TEST_ALL_PREFIXES(SyncSetupHandlerFirstSigninTest, DisplayBasicLogin);

  bool is_configuring_sync() const { return configuring_sync_; }

  // Called when configuring sync is done to close the dialog and start syncing.
  void ConfigureSyncDone();

  // Helper routine that gets the ProfileSyncService associated with the parent
  // profile.
  browser_sync::ProfileSyncService* GetSyncService() const;

  // Returns the LoginUIService for the parent profile.
  LoginUIService* GetLoginUIService() const;

 private:
  // Callbacks from the page.
  void OnDidClosePage(const base::ListValue* args);
  void HandleConfigure(const base::ListValue* args);
  void HandlePassphraseEntry(const base::ListValue* args);
  void HandlePassphraseCancel(const base::ListValue* args);
  void HandleShowSetupUI(const base::ListValue* args);
  void HandleAttemptUserExit(const base::ListValue* args);
  void HandleStartSignin(const base::ListValue* args);
  void HandleStopSyncing(const base::ListValue* args);
  void HandleCloseTimeout(const base::ListValue* args);
#if !defined(OS_CHROMEOS)
  // Displays the GAIA login form.
  void DisplayGaiaLogin(signin_metrics::AccessPoint access_point);

  // When web-flow is enabled, displays the Gaia login form in a new tab.
  // This function is virtual so that tests can override.
  virtual void DisplayGaiaLoginInNewTabOrWindow(
      signin_metrics::AccessPoint access_point);
#endif

  // Helper routine that gets the Profile associated with this object (virtual
  // so tests can override).
  virtual Profile* GetProfile() const;

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

  // Closes the associated sync settings page.
  void CloseUI();

  // Returns true if this object is the active login object.
  bool IsActiveLogin() const;

  // If a wizard already exists, return true. Otherwise, return false.
  bool IsExistingWizardPresent();

  // If a wizard already exists, focus it and return true.
  bool FocusExistingWizardIfPresent();

  // Display the configure sync UI. If |passphrase_failed| is true, the account
  // requires a passphrase and one hasn't been provided or it was invalid.
  void DisplayConfigureSync(bool passphrase_failed);

  // Helper object used to wait for the sync backend to startup.
  std::unique_ptr<SyncStartupTracker> sync_startup_tracker_;

  // Prevents Sync from running until configuration is complete.
  std::unique_ptr<syncer::SyncSetupInProgressHandle> sync_blocker_;

  // Set to true whenever the sync configure UI is visible. This is used to tell
  // what stage of the setup wizard the user was in and to update the UMA
  // histograms in the case that the user cancels out.
  bool configuring_sync_;

  // The OneShotTimer object used to timeout of starting the sync backend
  // service.
  std::unique_ptr<base::OneShotTimer> backend_start_timer_;

  DISALLOW_COPY_AND_ASSIGN(SyncSetupHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_SYNC_SETUP_HANDLER_H_
