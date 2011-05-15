// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_STATUS_NETWORK_MENU_BUTTON_H_
#define CHROME_BROWSER_CHROMEOS_STATUS_NETWORK_MENU_BUTTON_H_
#pragma once

#include <string>

#include "base/task.h"
#include "base/timer.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/customization_document.h"
#include "chrome/browser/chromeos/login/message_bubble.h"
#include "chrome/browser/chromeos/status/network_menu.h"
#include "chrome/browser/chromeos/status/status_area_button.h"
#include "ui/base/animation/throb_animation.h"

namespace gfx {
class Canvas;
}

namespace chromeos {

class StatusAreaHost;

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
                          public NetworkMenu,
                          public NetworkLibrary::NetworkDeviceObserver,
                          public NetworkLibrary::NetworkManagerObserver,
                          public NetworkLibrary::NetworkObserver,
                          public NetworkLibrary::CellularDataPlanObserver,
                          public MessageBubbleDelegate {
 public:
  explicit NetworkMenuButton(StatusAreaHost* host);
  virtual ~NetworkMenuButton();

  // ui::AnimationDelegate implementation.
  virtual void AnimationProgressed(const ui::Animation* animation);

  // NetworkLibrary::NetworkDeviceObserver implementation.
  virtual void OnNetworkDeviceChanged(NetworkLibrary* cros,
                                      const NetworkDevice* device);
  // NetworkLibrary::NetworkManagerObserver implementation.
  virtual void OnNetworkManagerChanged(NetworkLibrary* cros);
  // NetworkLibrary::NetworkObserver implementation.
  virtual void OnNetworkChanged(NetworkLibrary* cros, const Network* network);
  // NetworkLibrary::CellularDataPlanObserver implementation.
  virtual void OnCellularDataPlanChanged(NetworkLibrary* cros);

  // NetworkMenu implementation:
  virtual bool IsBrowserMode() const;

 protected:
  // NetworkMenu implementation:
  virtual gfx::NativeWindow GetNativeWindow() const;
  virtual void OpenButtonOptions();
  virtual bool ShouldOpenButtonOptions() const;

  // views::View
  virtual void OnLocaleChanged() OVERRIDE;

  // MessageBubbleDelegate implementation:
  virtual void BubbleClosing(Bubble* bubble, bool closed_by_escape);
  virtual bool CloseOnEscape();
  virtual bool FadeInOnShow();
  virtual void OnHelpLinkActivated();

 private:
  // Returns carrier deal if it's specified and should be shown,
  // otherwise returns NULL.
  const ServicesCustomizationDocument::CarrierDeal* GetCarrierDeal(
      NetworkLibrary* cros);

  // Sets the icon and the badges (badges are at the bottom of the icon).
  void SetIconAndBadges(const SkBitmap* icon,
                        const SkBitmap* right_badge,
                        const SkBitmap* left_badge);
  // Sets the icon only. Keep the previous badge.
  void SetIconOnly(const SkBitmap* icon);
  // Sets the badges only. Keep the previous icon.
  void SetBadgesOnly(const SkBitmap* right_badge, const SkBitmap* left_badge);
  // Set the network icon based on the status of the |network|
  void SetNetworkIcon(NetworkLibrary* cros, const Network* network);

  // Called when the list of devices has possibly changed. This will remove
  // old network device observers and add a network observers
  // for the new devices.
  void RefreshNetworkDeviceObserver(NetworkLibrary* cros);

  // Called when the active network has possibly changed. This will remove
  // old network observer and add a network observer for the active network.
  void RefreshNetworkObserver(NetworkLibrary* cros);

  // Shows 3G promo notification if needed.
  void ShowOptionalMobileDataPromoNotification(NetworkLibrary* cros);

  // Path of the Cellular device that we monitor property updates from.
  std::string cellular_device_path_;

  // The icon showing the network strength.
  const SkBitmap* icon_;
  // A badge icon displayed on top of icon, in bottom-right corner.
  const SkBitmap* right_badge_;
  // A  badge icon displayed on top of icon, in bottom-left corner.
  const SkBitmap* left_badge_;

  // Notification bubble for 3G promo.
  MessageBubble* mobile_data_bubble_;

  // True if check for promo needs to be done,
  // otherwise just ignore it for current session.
  bool check_for_promo_;

  // The throb animation that does the wifi connecting animation.
  ui::ThrobAnimation animation_connecting_;

  // The duration of the icon throbbing in milliseconds.
  static const int kThrobDuration;

  // If any network is currently active, this is the service path of the one
  // whose status is displayed in the network menu button.
  std::string active_network_;

  // Current carrier deal URL.
  std::string deal_url_;

  // Factory for delaying showing promo notification.
  ScopedRunnableMethodFactory<NetworkMenuButton> method_factory_;

  DISALLOW_COPY_AND_ASSIGN(NetworkMenuButton);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_STATUS_NETWORK_MENU_BUTTON_H_
