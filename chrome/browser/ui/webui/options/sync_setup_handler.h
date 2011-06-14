// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_SYNC_SETUP_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_SYNC_SETUP_HANDLER_H_

#include "chrome/browser/sync/sync_setup_flow_handler.h"
#include "chrome/browser/ui/webui/options/options_ui.h"

class SyncSetupFlow;

class SyncSetupHandler : public OptionsPageUIHandler,
                         public SyncSetupFlowHandler {
 public:
  SyncSetupHandler();
  virtual ~SyncSetupHandler();

  // OptionsPageUIHandler implementation.
  virtual void GetLocalizedValues(DictionaryValue* localized_strings);
  virtual void Initialize();
  virtual void RegisterMessages();

  // SyncSetupFlowHandler implementation.
  virtual void ShowGaiaLogin(const DictionaryValue& args);
  virtual void ShowGaiaSuccessAndClose();
  virtual void ShowGaiaSuccessAndSettingUp();
  virtual void ShowConfigure(const DictionaryValue& args);
  virtual void ShowPassphraseEntry(const DictionaryValue& args);
  virtual void ShowSettingUp();
  virtual void ShowSetupDone(const std::wstring& user);

  virtual void SetFlow(SyncSetupFlow* flow);

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
  void OnDidClosePage(const ListValue* args);
  void HandleSubmitAuth(const ListValue* args);
  void HandleConfigure(const ListValue* args);
  void HandlePassphraseEntry(const ListValue* args);
  void HandlePassphraseCancel(const ListValue* args);
  void HandleAttachHandler(const ListValue* args);
  void HandleShowErrorUI(const ListValue* args);

  SyncSetupFlow* flow() { return flow_; }

 private:
  // Weak reference.
  SyncSetupFlow* flow_;

  DISALLOW_COPY_AND_ASSIGN(SyncSetupHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_SYNC_SETUP_HANDLER_H_
