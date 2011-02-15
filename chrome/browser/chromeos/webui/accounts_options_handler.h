// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_WEBUI_ACCOUNTS_OPTIONS_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_WEBUI_ACCOUNTS_OPTIONS_HANDLER_H_
#pragma once

#include "chrome/browser/chromeos/webui/cros_options_page_ui_handler.h"

namespace chromeos {

class UserCrosSettingsProvider;

// ChromeOS accounts options page handler.
class AccountsOptionsHandler : public CrosOptionsPageUIHandler {
 public:
  AccountsOptionsHandler();
  virtual ~AccountsOptionsHandler();

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages();

  // OptionsUIHandler implementation:
  virtual void GetLocalizedValues(DictionaryValue* localized_strings);

 private:
  UserCrosSettingsProvider* users_settings() const;

  // Javascript callbacks to whitelist/unwhitelist user.
  void WhitelistUser(const ListValue* args);
  void UnwhitelistUser(const ListValue* args);

  // Javascript callback to fetch known user pictures.
  void FetchUserPictures(const ListValue* args);

  // Javascript callback to auto add existing users to white list.
  void WhitelistExistingUsers(const ListValue* args);

  DISALLOW_COPY_AND_ASSIGN(AccountsOptionsHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_WEBUI_ACCOUNTS_OPTIONS_HANDLER_H_

