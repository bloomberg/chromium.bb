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
  virtual void ShowFirstPassphrase(const DictionaryValue& args);
  virtual void ShowSettingUp();
  virtual void ShowSetupDone(const std::wstring& user);
  virtual void ShowFirstTimeDone(const std::wstring& user);

  virtual void SetFlow(SyncSetupFlow* flow) {
    flow_ = flow;
  }

 private:
  // Callbacks from the page.
  void OnDidShowPage(const ListValue* args);
  void OnDidClosePage(const ListValue* args);
  void HandleSubmitAuth(const ListValue* args);
  void HandleConfigure(const ListValue* args);
  void HandlePassphraseEntry(const ListValue* args);
  void HandlePassphraseCancel(const ListValue* args);
  void HandleFirstPassphrase(const ListValue* args);
  void HandleGoToDashboard(const ListValue* args);

  // Weak reference.
  SyncSetupFlow* flow_;

  DISALLOW_COPY_AND_ASSIGN(SyncSetupHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_SYNC_SETUP_HANDLER_H_
