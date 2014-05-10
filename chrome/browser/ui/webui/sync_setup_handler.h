// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SYNC_SETUP_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SYNC_SETUP_HANDLER_H_

#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/timer/timer.h"
#include "chrome/browser/sync/sync_startup_tracker.h"
#include "chrome/browser/ui/webui/options/options_ui.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"

class LoginUIService;
class ProfileManager;
class ProfileSyncService;
class SigninManagerBase;

namespace content {
class WebContents;
}

class SyncSetupHandler : public options::OptionsPageUIHandler,
                         public SyncStartupTracker::Observer,
                         public LoginUIService::LoginUI {
 public:
  // Constructs a new SyncSetupHandler. |profile_manager| may be NULL.
  explicit SyncSetupHandler(ProfileManager* profile_manager);
  virtual ~SyncSetupHandler();

  // OptionsPageUIHandler implementation.
  virtual void GetLocalizedValues(base::DictionaryValue* localized_strings)
      OVERRIDE;
  virtual void RegisterMessages() OVERRIDE;

  // SyncStartupTracker::Observer implementation;
  virtual void SyncStartupCompleted() OVERRIDE;
  virtual void SyncStartupFailed() OVERRIDE;

  // LoginUIService::LoginUI implementation.
  virtual void FocusUI() OVERRIDE;
  virtual void CloseUI() OVERRIDE;

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
  void HandleShowSetupUI(const base::ListValue* args);
  void HandleDoSignOutOnAuthError(const base::ListValue* args);
  void HandleStartSignin(const base::ListValue* args);
  void HandleStopSyncing(const base::ListValue* args);
  void HandleCloseTimeout(const base::ListValue* args);
#if !defined(OS_CHROMEOS)
  // Displays the GAIA login form.
  void DisplayGaiaLogin();

  // When web-flow is enabled, displays the Gaia login form in a new tab.
  // This function is virtual so that tests can override.
  virtual void DisplayGaiaLoginInNewTabOrWindow();
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

  // Returns true if this object is the active login object.
  bool IsActiveLogin() const;

  // If a wizard already exists, return true. Otherwise, return false.
  bool IsExistingWizardPresent();

  // If a wizard already exists, focus it and return true.
  bool FocusExistingWizardIfPresent();

  // Helper object used to wait for the sync backend to startup.
  scoped_ptr<SyncStartupTracker> sync_startup_tracker_;

  // Set to true whenever the sync configure UI is visible. This is used to tell
  // what stage of the setup wizard the user was in and to update the UMA
  // histograms in the case that the user cancels out.
  bool configuring_sync_;

  // Weak reference to the profile manager.
  ProfileManager* const profile_manager_;

  // The OneShotTimer object used to timeout of starting the sync backend
  // service.
  scoped_ptr<base::OneShotTimer<SyncSetupHandler> > backend_start_timer_;

  DISALLOW_COPY_AND_ASSIGN(SyncSetupHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_SYNC_SETUP_HANDLER_H_
