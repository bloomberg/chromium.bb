// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_CHROMEOS_NETWORK_TRAY_VPN_H
#define ASH_SYSTEM_CHROMEOS_NETWORK_TRAY_VPN_H

#include <memory>

#include "ash/system/chromeos/network/tray_network_state_observer.h"
#include "ash/system/tray/system_tray_item.h"
#include "base/macros.h"

namespace ash {
class TrayNetworkStateObserver;

namespace tray {
class NetworkDetailedView;
class VpnDefaultView;
class VpnDetailedView;
}

class TrayVPN : public SystemTrayItem,
                public TrayNetworkStateObserver::Delegate {
 public:
  explicit TrayVPN(SystemTray* system_tray);
  ~TrayVPN() override;

  // SystemTrayItem
  views::View* CreateTrayView(user::LoginStatus status) override;
  views::View* CreateDefaultView(user::LoginStatus status) override;
  views::View* CreateDetailedView(user::LoginStatus status) override;
  void DestroyTrayView() override;
  void DestroyDefaultView() override;
  void DestroyDetailedView() override;
  void UpdateAfterLoginStatusChange(user::LoginStatus status) override;
  void UpdateAfterShelfAlignmentChange(wm::ShelfAlignment alignment) override;

  // TrayNetworkStateObserver::Delegate
  void NetworkStateChanged() override;

 private:
  tray::VpnDefaultView* default_;
  tray::NetworkDetailedView* detailed_;
  std::unique_ptr<TrayNetworkStateObserver> network_state_observer_;

  DISALLOW_COPY_AND_ASSIGN(TrayVPN);
};

}  // namespace ash

#endif  // ASH_SYSTEM_CHROMEOS_NETWORK_TRAY_VPN_H
