// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
#include "chrome/browser/chromeos/mobile_config.h"
#include "chrome/browser/chromeos/status/network_menu.h"
#include "chrome/browser/chromeos/status/network_menu_icon.h"
#include "chrome/browser/chromeos/status/status_area_button.h"

class PrefService;

namespace chromeos {

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
                          public views::ViewMenuDelegate,
                          public NetworkMenu::Delegate,
                          public NetworkMenuIcon::Delegate,
                          public NetworkLibrary::NetworkDeviceObserver,
                          public NetworkLibrary::NetworkManagerObserver,
                          public NetworkLibrary::NetworkObserver,
                          public NetworkLibrary::CellularDataPlanObserver,
                          public views::Widget::Observer,
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

  // views::ViewMenuDelegate implementation.
  virtual void RunMenu(views::View* source, const gfx::Point& pt) OVERRIDE;

  // views::Widget::Observer implementation:
  virtual void OnWidgetClosing(views::Widget* widget) OVERRIDE;

  // MessageBubbleLinkListener implementation:
  virtual void OnLinkActivated(size_t index) OVERRIDE;

 private:
  // Returns carrier info.
  const MobileConfig::Carrier* GetCarrier(NetworkLibrary* cros);

  // Returns carrier deal if it's specified and should be shown,
  // otherwise returns NULL.
  const MobileConfig::CarrierDeal* GetCarrierDeal(
      const MobileConfig::Carrier* carrier);

  // Set the network icon based on the status of the |network|
  void SetNetworkIcon();

  // Called when the list of devices has possibly changed. This will remove
  // old network device observers and add a network observers
  // for the new devices.
  void RefreshNetworkDeviceObserver(NetworkLibrary* cros);

  // Called when the active network has possibly changed. This will remove
  // old network observer and add a network observer for the active network.
  void RefreshNetworkObserver(NetworkLibrary* cros);

  // Shows 3G promo notification if needed.
  void ShowOptionalMobileDataPromoNotification(NetworkLibrary* cros);

  void SetTooltipAndAccessibleName(const string16& label);

  // The Network menu.
  scoped_ptr<NetworkMenu> network_menu_;

  // Path of the Cellular device that we monitor property updates from.
  std::string cellular_device_path_;

  // The network icon and associated data.
  scoped_ptr<NetworkMenuIcon> network_icon_;

  // Notification bubble for 3G promo.
  MessageBubble* mobile_data_bubble_;

  // True if check for promo needs to be done,
  // otherwise just ignore it for current session.
  bool check_for_promo_;

  // Cellular device SIM was locked when we last checked
  bool was_sim_locked_;

  // If any network is currently active, this is the service path of the one
  // whose status is displayed in the network menu button.
  std::string active_network_;

  // Current carrier deal info URL.
  std::string deal_info_url_;

  // Current carrier deal top-up URL.
  std::string deal_topup_url_;

  // Factory for delaying showing promo notification.
  base::WeakPtrFactory<NetworkMenuButton> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(NetworkMenuButton);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_STATUS_NETWORK_MENU_BUTTON_H_
