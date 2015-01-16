// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_ACCOUNTS_OPTIONS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_ACCOUNTS_OPTIONS_HANDLER_H_

#include "base/compiler_specific.h"
#include "chrome/browser/ui/webui/options/options_ui.h"

namespace chromeos {
namespace options {

// ChromeOS accounts options page handler.
class AccountsOptionsHandler : public ::options::OptionsPageUIHandler {
 public:
  AccountsOptionsHandler();
  ~AccountsOptionsHandler() override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;

  // OptionsPageUIHandler implementation.
  void GetLocalizedValues(base::DictionaryValue* localized_strings) override;

 private:
  // Javascript callbacks to update whitelist/unwhitelist user.
  void HandleWhitelistUser(const base::ListValue* args);
  void HandleUnwhitelistUser(const base::ListValue* args);

  // Javascript callback to update the white list: auto add existing users,
  // remove not present supervised users.
  void HandleUpdateWhitelist(const base::ListValue* args);

  DISALLOW_COPY_AND_ASSIGN(AccountsOptionsHandler);
};

}  // namespace options
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_ACCOUNTS_OPTIONS_HANDLER_H_
