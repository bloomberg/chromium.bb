// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_NETWORK_DROPDOWN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_NETWORK_DROPDOWN_HANDLER_H_

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"

namespace chromeos {

class NetworkDropdown;

class NetworkDropdownHandler : public BaseScreenHandler {
 public:
  NetworkDropdownHandler();
  virtual ~NetworkDropdownHandler();

  // BaseScreenHandler implementation:
  virtual void GetLocalizedStrings(
      base::DictionaryValue* localized_strings) OVERRIDE;
  virtual void Initialize() OVERRIDE;

  // WebUIMessageHandler implementation:
  virtual void RegisterMessages() OVERRIDE;

 private:
  // Handles choosing of the network menu item.
  void HandleNetworkItemChosen(const base::ListValue* args);
  // Handles network drop-down showing.
  void HandleNetworkDropdownShow(const base::ListValue* args);
  // Handles network drop-down hiding.
  void HandleNetworkDropdownHide(const base::ListValue* args);

  scoped_ptr<NetworkDropdown> dropdown_;

  DISALLOW_COPY_AND_ASSIGN(NetworkDropdownHandler);
};

}  // namespace chromeos
#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_NETWORK_DROPDOWN_HANDLER_H_
