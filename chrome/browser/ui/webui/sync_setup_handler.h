// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SYNC_SETUP_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SYNC_SETUP_HANDLER_H_

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/net/gaia/gaia_oauth_consumer.h"
#include "chrome/browser/net/gaia/gaia_oauth_fetcher.h"
#include "chrome/browser/sync/sync_setup_flow_handler.h"
#include "chrome/browser/ui/webui/options/options_ui.h"

class SyncSetupFlow;
class ProfileManager;

class SyncSetupHandler : public GaiaOAuthConsumer,
                         public OptionsPageUIHandler,
                         public SyncSetupFlowHandler {
 public:
  // Constructs a new SyncSetupHandler. |profile_manager| may be NULL.
  explicit SyncSetupHandler(ProfileManager* profile_manager);
  virtual ~SyncSetupHandler();

  // OptionsPageUIHandler implementation.
  virtual void GetLocalizedValues(base::DictionaryValue* localized_strings)
      OVERRIDE;
  virtual void Initialize() OVERRIDE;
  virtual void RegisterMessages() OVERRIDE;

  // SyncSetupFlowHandler implementation.
  virtual void ShowOAuthLogin() OVERRIDE;
  virtual void ShowGaiaLogin(const base::DictionaryValue& args) OVERRIDE;
  virtual void ShowGaiaSuccessAndClose() OVERRIDE;
  virtual void ShowGaiaSuccessAndSettingUp() OVERRIDE;
  virtual void ShowConfigure(const base::DictionaryValue& args) OVERRIDE;
  virtual void ShowPassphraseEntry(const base::DictionaryValue& args) OVERRIDE;
  virtual void ShowSettingUp() OVERRIDE;
  virtual void ShowSetupDone(const string16& user) OVERRIDE;
  virtual void SetFlow(SyncSetupFlow* flow) OVERRIDE;
  virtual void Focus() OVERRIDE;

  // GaiaOAuthConsumer implementation.
  virtual void OnGetOAuthTokenSuccess(const std::string& oauth_token) OVERRIDE;
  virtual void OnGetOAuthTokenFailure(
      const GoogleServiceAuthError& error) OVERRIDE;

  static void GetStaticLocalizedValues(
      base::DictionaryValue* localized_strings,
      WebUI* web_ui);

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

 private:
  // If a wizard already exists, focus it and return true.
  bool FocusExistingWizard();

  // Invokes the javascript call to close the setup overlay.
  void CloseOverlay();

  // Returns true if the given login data is valid, false otherwise. If the
  // login data is not valid then on return |error_message| will be set to  a
  // localized error message. Note, |error_message| must not be NULL.
  bool IsLoginAuthDataValid(const std::string& username,
                            string16* error_message);

  // Displays the given error message in the login UI.
  void ShowLoginErrorMessage(const string16& error_message);

  // Weak reference.
  SyncSetupFlow* flow_;
  scoped_ptr<GaiaOAuthFetcher> oauth_login_;
  // Weak reference to the profile manager.
  ProfileManager* const profile_manager_;

  DISALLOW_COPY_AND_ASSIGN(SyncSetupHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_SYNC_SETUP_HANDLER_H_
