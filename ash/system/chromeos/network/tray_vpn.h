// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_CHROMEOS_NETWORK_TRAY_VPN_H
#define ASH_SYSTEM_CHROMEOS_NETWORK_TRAY_VPN_H

#include "ash/system/chromeos/network/tray_network_state_observer.h"
#include "ash/system/tray/system_tray_item.h"
#include "base/memory/scoped_ptr.h"

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
  virtual ~TrayVPN();

  // SystemTrayItem
  virtual views::View* CreateTrayView(user::LoginStatus status) OVERRIDE;
  virtual views::View* CreateDefaultView(user::LoginStatus status) OVERRIDE;
  virtual views::View* CreateDetailedView(user::LoginStatus status) OVERRIDE;
  virtual void DestroyTrayView() OVERRIDE;
  virtual void DestroyDefaultView() OVERRIDE;
  virtual void DestroyDetailedView() OVERRIDE;
  virtual void UpdateAfterLoginStatusChange(user::LoginStatus status) OVERRIDE;
  virtual void UpdateAfterShelfAlignmentChange(
      ShelfAlignment alignment) OVERRIDE;

  // TrayNetworkStateObserver::Delegate
  virtual void NetworkStateChanged(bool list_changed) OVERRIDE;
  virtual void NetworkServiceChanged(
      const chromeos::NetworkState* network) OVERRIDE;

 private:
  tray::VpnDefaultView* default_;
  tray::NetworkDetailedView* detailed_;
  scoped_ptr<TrayNetworkStateObserver> network_state_observer_;

  DISALLOW_COPY_AND_ASSIGN(TrayVPN);
};

}  // namespace ash

#endif  // ASH_SYSTEM_CHROMEOS_NETWORK_TRAY_VPN_H
