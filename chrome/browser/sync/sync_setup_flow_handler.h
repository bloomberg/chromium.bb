// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_SYNC_SETUP_FLOW_HANDLER_H_
#define CHROME_BROWSER_SYNC_SYNC_SETUP_FLOW_HANDLER_H_
#pragma once

#include <string>

#include "content/browser/webui/web_ui.h"

class DictionaryValue;
class SyncSetupFlow;

class SyncSetupFlowHandler {
 public:
  // These functions control which part of the HTML is visible.
  virtual void ShowGaiaLogin(const DictionaryValue& args) = 0;
  virtual void ShowGaiaSuccessAndClose() = 0;
  virtual void ShowGaiaSuccessAndSettingUp() = 0;
  virtual void ShowConfigure(const DictionaryValue& args) = 0;
  virtual void ShowPassphraseEntry(const DictionaryValue& args) = 0;
  virtual void ShowFirstPassphrase(const DictionaryValue& args) = 0;
  virtual void ShowSettingUp() = 0;
  virtual void ShowSetupDone(const std::wstring& user) = 0;
  virtual void ShowFirstTimeDone(const std::wstring& user) = 0;

  virtual void SetFlow(SyncSetupFlow* flow) = 0;

 protected:
  virtual ~SyncSetupFlowHandler() {}
};

#endif  // CHROME_BROWSER_SYNC_SYNC_SETUP_FLOW_HANDLER_H_
