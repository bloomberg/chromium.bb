// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_NETWORK_DROPDOWN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_NETWORK_DROPDOWN_HANDLER_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/network_dropdown.h"

namespace chromeos {

class NetworkDropdownHandler : public BaseScreenHandler,
                               public NetworkDropdown::Actor {
 public:
  class Observer {
   public:
    virtual ~Observer() {}
    virtual void OnConnectToNetworkRequested() = 0;
  };

  NetworkDropdownHandler();
  virtual ~NetworkDropdownHandler();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // BaseScreenHandler implementation:
  virtual void DeclareLocalizedValues(LocalizedValuesBuilder* builder) OVERRIDE;
  virtual void Initialize() OVERRIDE;

  // WebUIMessageHandler implementation:
  virtual void RegisterMessages() OVERRIDE;

 private:
  // NetworkDropdown::Actor implementation:
  virtual void OnConnectToNetworkRequested() OVERRIDE;

  // Handles choosing of the network menu item.
  void HandleNetworkItemChosen(double id);
  // Handles network drop-down showing.
  void HandleNetworkDropdownShow(const std::string& element_id,
                                 bool oobe);
  // Handles network drop-down hiding.
  void HandleNetworkDropdownHide();
  // Handles network drop-down refresh.
  void HandleNetworkDropdownRefresh();

  scoped_ptr<NetworkDropdown> dropdown_;

  ObserverList<Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(NetworkDropdownHandler);
};

}  // namespace chromeos
#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_NETWORK_DROPDOWN_HANDLER_H_
