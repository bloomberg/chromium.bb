// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_STATUS_NETWORK_MENU_BUTTON_H_
#define CHROME_BROWSER_CHROMEOS_STATUS_NETWORK_MENU_BUTTON_H_
#pragma once

#include <string>

#include "base/memory/weak_ptr.h"
#include "base/timer.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/login/message_bubble.h"
#include "chrome/browser/chromeos/status/network_menu.h"
#include "chrome/browser/chromeos/status/network_menu_icon.h"
#include "chrome/browser/chromeos/status/status_area_button.h"

class PrefService;

namespace chromeos {

class DataPromoNotification;

// The network menu button in the status area.
// This class will handle getting the wifi networks and populating the menu.
// It will also handle the status icon changing and connecting to another
// wifi/cellular network.
//
// The network menu looks like this:
//
// <icon>  Ethernet
// <icon>  Wifi Network A
// <icon>  Wifi Network B
// <icon>  Wifi Network C
// <icon>  Cellular Network A
// <icon>  Cellular Network B
// <icon>  Cellular Network C
// <icon>  Other...
// --------------------------------
//         Disable Wifi
//         Disable Celluar
// --------------------------------
//         <IP Address>
//         Network settings...
//
// <icon> will show the strength of the wifi/cellular networks.
// The label will be BOLD if the network is currently connected.
class NetworkMenuButton : public StatusAreaButton,
                          public views::MenuButtonListener,
                          public NetworkMenu::Delegate,
                          public NetworkMenuIcon::Delegate,
                          public NetworkLibrary::NetworkDeviceObserver,
                          public NetworkLibrary::NetworkManagerObserver,
                          public NetworkLibrary::NetworkObserver,
                          public NetworkLibrary::CellularDataPlanObserver,
                          public MessageBubbleLinkListener {
 public:
  explicit NetworkMenuButton(StatusAreaButton::Delegate* delegate);
  virtual ~NetworkMenuButton();

  static void RegisterPrefs(PrefService* local_state);

  // NetworkLibrary::NetworkDeviceObserver implementation.
  virtual void OnNetworkDeviceChanged(NetworkLibrary* cros,
                                      const NetworkDevice* device) OVERRIDE;

  // NetworkLibrary::NetworkManagerObserver implementation.
  virtual void OnNetworkManagerChanged(NetworkLibrary* cros) OVERRIDE;

  // NetworkLibrary::NetworkObserver implementation.
  virtual void OnNetworkChanged(NetworkLibrary* cros,
                                const Network* network) OVERRIDE;

  // NetworkLibrary::CellularDataPlanObserver implementation.
  virtual void OnCellularDataPlanChanged(NetworkLibrary* cros) OVERRIDE;

  // NetworkMenu::Delegate implementation:
  virtual views::MenuButton* GetMenuButton() OVERRIDE;
  virtual gfx::NativeWindow GetNativeWindow() const OVERRIDE;
  virtual void OpenButtonOptions() OVERRIDE;
  virtual bool ShouldOpenButtonOptions() const OVERRIDE;

  // NetworkMenuIcon::Delegate implementation:
  virtual void NetworkMenuIconChanged() OVERRIDE;

  // views::View
  virtual void OnLocaleChanged() OVERRIDE;

  // views::MenuButtonListener implementation.
  virtual void OnMenuButtonClicked(views::View* source,
                                   const gfx::Point& point) OVERRIDE;

  // MessageBubbleLinkListener implementation:
  virtual void OnLinkActivated(size_t index) OVERRIDE;

 private:
  // Set the network icon based on the status of the |network|
  void SetNetworkIcon();

  // Called when the list of devices has possibly changed. This will remove
  // old network device observers and add a network observers
  // for the new devices.
  void RefreshNetworkDeviceObserver(NetworkLibrary* cros);

  // Called when the active network has possibly changed. This will remove
  // old network observer and add a network observer for the active network.
  void RefreshNetworkObserver(NetworkLibrary* cros);

  void SetTooltipAndAccessibleName(const string16& label);

  // The Network menu.
  scoped_ptr<NetworkMenu> network_menu_;

  // Path of the Cellular device that we monitor property updates from.
  std::string cellular_device_path_;

  // The network icon and associated data.
  scoped_ptr<NetworkMenuIcon> network_icon_;

  // If any network is currently active, this is the service path of the one
  // whose status is displayed in the network menu button.
  std::string active_network_;

  scoped_ptr<DataPromoNotification> data_promo_notification_;

  DISALLOW_COPY_AND_ASSIGN(NetworkMenuButton);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_STATUS_NETWORK_MENU_BUTTON_H_
