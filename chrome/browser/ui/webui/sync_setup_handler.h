// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SYNC_SETUP_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SYNC_SETUP_HANDLER_H_

#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/signin/signin_tracker.h"
#include "chrome/browser/sync/sync_setup_flow_handler.h"
#include "chrome/browser/ui/webui/options/options_ui.h"

class LoginUIService;
class ProfileManager;
class ProfileSyncService;
class SigninManager;
class SyncSetupFlow;

class SyncSetupHandler : public OptionsPageUIHandler,
                         public SyncSetupFlowHandler,
                         public SigninTracker::Observer {
 public:
  // Constructs a new SyncSetupHandler. |profile_manager| may be NULL.
  explicit SyncSetupHandler(ProfileManager* profile_manager);
  virtual ~SyncSetupHandler();

  // OptionsPageUIHandler implementation.
  virtual void GetLocalizedValues(base::DictionaryValue* localized_strings)
      OVERRIDE;
  virtual void RegisterMessages() OVERRIDE;

  // SyncSetupFlowHandler implementation.
  virtual void ShowConfigure(const base::DictionaryValue& args) OVERRIDE;
  virtual void ShowFatalError() OVERRIDE;
  virtual void ShowPassphraseEntry(const base::DictionaryValue& args) OVERRIDE;
  virtual void ShowSettingUp() OVERRIDE;
  virtual void ShowSetupDone(const string16& user) OVERRIDE;
  virtual void SetFlow(SyncSetupFlow* flow) OVERRIDE;
  virtual void Focus() OVERRIDE;

  // SigninTracker::Observer implementation
  virtual void GaiaCredentialsValid() OVERRIDE;
  virtual void SigninFailed(const GoogleServiceAuthError& error) OVERRIDE;
  virtual void SigninSuccess() OVERRIDE;

  static void GetStaticLocalizedValues(
      base::DictionaryValue* localized_strings,
      content::WebUI* web_ui);

  // Initializes the sync setup flow and shows the setup UI.
  void OpenSyncSetup();

  // Terminates the sync setup flow.
  void CloseSyncSetup();

 protected:
  FRIEND_TEST_ALL_PREFIXES(SyncSetupWizardTest, InitialStepLogin);
  FRIEND_TEST_ALL_PREFIXES(SyncSetupWizardTest, ChooseDataTypesSetsPrefs);
  FRIEND_TEST_ALL_PREFIXES(SyncSetupWizardTest, DialogCancelled);
  FRIEND_TEST_ALL_PREFIXES(SyncSetupWizardTest, InvalidTransitions);
  FRIEND_TEST_ALL_PREFIXES(SyncSetupWizardTest, FullSuccessfulRunSetsPref);
  FRIEND_TEST_ALL_PREFIXES(SyncSetupWizardTest, AbortedByPendingClear);
  FRIEND_TEST_ALL_PREFIXES(SyncSetupWizardTest, DiscreteRunGaiaLogin);
  FRIEND_TEST_ALL_PREFIXES(SyncSetupWizardTest, DiscreteRunChooseDataTypes);
  FRIEND_TEST_ALL_PREFIXES(SyncSetupWizardTest,
                           DiscreteRunChooseDataTypesAbortedByPendingClear);
  FRIEND_TEST_ALL_PREFIXES(SyncSetupWizardTest, EnterPassphraseRequired);
  FRIEND_TEST_ALL_PREFIXES(SyncSetupHandlerTest, GaiaErrorInitializingSync);
  FRIEND_TEST_ALL_PREFIXES(SyncSetupHandlerTest, HandleCaptcha);
  FRIEND_TEST_ALL_PREFIXES(SyncSetupHandlerTest, HandleGaiaAuthFailure);
  FRIEND_TEST_ALL_PREFIXES(SyncSetupHandlerTest,
                           UnrecoverableErrorInitializingSync);

  // Callbacks from the page. Protected in order to be called by the
  // SyncSetupWizardTest.
  void OnDidClosePage(const base::ListValue* args);
  void HandleSubmitAuth(const base::ListValue* args);
  void HandleConfigure(const base::ListValue* args);
  void HandlePassphraseEntry(const base::ListValue* args);
  void HandlePassphraseCancel(const base::ListValue* args);
  void HandleAttachHandler(const base::ListValue* args);
  void HandleShowErrorUI(const base::ListValue* args);
  void HandleShowSetupUI(const base::ListValue* args);

  SyncSetupFlow* flow() { return flow_; }

  // Subclasses must implement this to show the setup UI that's appropriate
  // for the page this is contained in.
  virtual void ShowSetupUI() = 0;

  // Overridden by subclasses (like SyncPromoHandler) to log stats about the
  // user's signin activity.
  virtual void RecordSignin();

 private:
  // Helper routine that gets the Profile associated with this object (virtual
  // so tests can override).
  virtual Profile* GetProfile() const;

  // Start up the sync setup configuration wizard.
  void StartConfigureSync();

  // Shows the GAIA login success page then exits.
  void DisplayGaiaSuccessAndClose();

  // Displays the GAIA login success page then transitions to sync setup.
  void DisplayGaiaSuccessAndSettingUp();

  // Displays the GAIA login form. If |fatal_error| is true, displays the fatal
  // error UI.
  void DisplayGaiaLogin(bool fatal_error);

  // Displays the GAIA login form with a custom error message (used for errors
  // like "email address already in use by another profile"). No message
  // displayed if |error_message| is empty. Displays fatal error UI if
  // |fatal_error| = true.
  void DisplayGaiaLoginWithErrorMessage(const string16& error_message,
                                        bool fatal_error);

  // Returns true if we're the active login object.
  bool IsActiveLogin() const;

  // Initiates a login via the signin manager.
  void TryLogin(const std::string& username,
                const std::string& password,
                const std::string& captcha,
                const std::string& access_code);

  // If a wizard already exists, focus it and return true.
  bool FocusExistingWizardIfPresent();

  // Invokes the javascript call to close the setup overlay.
  void CloseOverlay();

  // Returns true if the given login data is valid, false otherwise. If the
  // login data is not valid then on return |error_message| will be set to  a
  // localized error message. Note, |error_message| must not be NULL.
  bool IsLoginAuthDataValid(const std::string& username,
                            string16* error_message);

  // Helper routine that gets the ProfileSyncService associated with the parent
  // profile.
  ProfileSyncService* GetSyncService() const;

  // Returns the SigninManager for the parent profile.
  SigninManager* GetSignin() const;

  // Returns the LoginUIService for the parent profile.
  LoginUIService* GetLoginUIService() const;

  // The SigninTracker object used to determine when the user has fully signed
  // in (this requires waiting for various services to initialize and tracking
  // errors from multiple sources). Should only be non-null while the login UI
  // is visible.
  scoped_ptr<SigninTracker> signin_tracker_;

  // Weak reference.
  SyncSetupFlow* flow_;
  // Weak reference to the profile manager.
  ProfileManager* const profile_manager_;

  // Cache of the last name the client attempted to authenticate.
  std::string last_attempted_user_email_;

  // The error from the last signin attempt.
  GoogleServiceAuthError last_signin_error_;

  DISALLOW_COPY_AND_ASSIGN(SyncSetupHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_SYNC_SETUP_HANDLER_H_
