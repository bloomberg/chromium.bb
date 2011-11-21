// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_SYNC_SETUP_FLOW_HANDLER_H_
#define CHROME_BROWSER_SYNC_SYNC_SETUP_FLOW_HANDLER_H_
#pragma once

#include <string>

class SyncSetupFlow;

namespace base {
class DictionaryValue;
}

class SyncSetupFlowHandler {
 public:
  // These functions control which part of the HTML is visible.
  virtual void ShowOAuthLogin() = 0;
  virtual void ShowGaiaLogin(const base::DictionaryValue& args) = 0;
  virtual void ShowGaiaSuccessAndClose() = 0;
  virtual void ShowGaiaSuccessAndSettingUp() = 0;
  virtual void ShowConfigure(const base::DictionaryValue& args) = 0;
  virtual void ShowPassphraseEntry(const base::DictionaryValue& args) = 0;
  virtual void ShowSettingUp() = 0;
  virtual void ShowSetupDone(const string16& user) = 0;
  virtual void SetFlow(SyncSetupFlow* flow) = 0;
  virtual void Focus() = 0;

 protected:
  virtual ~SyncSetupFlowHandler() {}
};

#endif  // CHROME_BROWSER_SYNC_SYNC_SETUP_FLOW_HANDLER_H_
