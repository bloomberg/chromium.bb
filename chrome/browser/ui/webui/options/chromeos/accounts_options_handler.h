// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_ACCOUNTS_OPTIONS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_ACCOUNTS_OPTIONS_HANDLER_H_
#pragma once

#include "base/compiler_specific.h"
#include "chrome/browser/ui/webui/options/options_ui.h"

namespace chromeos {

class UserCrosSettingsProvider;

// ChromeOS accounts options page handler.
class AccountsOptionsHandler : public OptionsPageUIHandler {
 public:
  AccountsOptionsHandler();
  virtual ~AccountsOptionsHandler();

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

  // OptionsPageUIHandler implementation.
  virtual void GetLocalizedValues(
      base::DictionaryValue* localized_strings) OVERRIDE;

 private:
  // Javascript callbacks to whitelist/unwhitelist user.
  void WhitelistUser(const base::ListValue* args);
  void UnwhitelistUser(const base::ListValue* args);

  // Javascript callback to auto add existing users to white list.
  void WhitelistExistingUsers(const base::ListValue* args);

  DISALLOW_COPY_AND_ASSIGN(AccountsOptionsHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_ACCOUNTS_OPTIONS_HANDLER_H_
