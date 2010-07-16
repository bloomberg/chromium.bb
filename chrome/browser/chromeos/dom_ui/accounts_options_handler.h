// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DOM_UI_ACCOUNTS_OPTIONS_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_DOM_UI_ACCOUNTS_OPTIONS_HANDLER_H_

#include "chrome/browser/dom_ui/options_ui.h"

namespace chromeos {

// ChromeOS accounts options page handler.
class AccountsOptionsHandler : public OptionsPageUIHandler {
 public:
  AccountsOptionsHandler();
  virtual ~AccountsOptionsHandler();

  // OptionsUIHandler implementation:
  virtual void GetLocalizedValues(DictionaryValue* localized_strings);

 private:
  DISALLOW_COPY_AND_ASSIGN(AccountsOptionsHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_DOM_UI_ACCOUNTS_OPTIONS_HANDLER_H_

