// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_CHROMEOS_NETWORK_TRAY_NETWORK_H
#define ASH_SYSTEM_CHROMEOS_NETWORK_TRAY_NETWORK_H

#include <set>

#include "ash/system/chromeos/network/network_observer.h"
#include "ash/system/chromeos/network/network_portal_detector_observer.h"
#include "ash/system/chromeos/network/tray_network_state_observer.h"
#include "ash/system/tray/system_tray_item.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"

namespace chromeos {
class NetworkState;
}

namespace ash {
namespace tray {
class NetworkDefaultView;
class NetworkDetailedView;
class NetworkTrayView;
}

class TrayNetwork : public SystemTrayItem,
                    public NetworkObserver,
                    public NetworkPortalDetectorObserver,
                    public TrayNetworkStateObserver::Delegate {
 public:
  explicit TrayNetwork(SystemTray* system_tray);
  virtual ~TrayNetwork();

  tray::NetworkDetailedView* detailed() { return detailed_; }

  // SystemTrayItem
  virtual views::View* CreateTrayView(user::LoginStatus status) override;
  virtual views::View* CreateDefaultView(user::LoginStatus status) override;
  virtual views::View* CreateDetailedView(user::LoginStatus status) override;
  virtual void DestroyTrayView() override;
  virtual void DestroyDefaultView() override;
  virtual void DestroyDetailedView() override;
  virtual void UpdateAfterLoginStatusChange(user::LoginStatus status) override;
  virtual void UpdateAfterShelfAlignmentChange(
      ShelfAlignment alignment) override;

  // NetworkObserver
  virtual void RequestToggleWifi() override;

  // NetworkPortalDetectorObserver
  virtual void OnCaptivePortalDetected(
      const std::string& service_path) override;

  // TrayNetworkStateObserver::Delegate
  virtual void NetworkStateChanged(bool list_changed) override;
  virtual void NetworkServiceChanged(
      const chromeos::NetworkState* network) override;

 private:
  tray::NetworkTrayView* tray_;
  tray::NetworkDefaultView* default_;
  tray::NetworkDetailedView* detailed_;
  bool request_wifi_view_;
  scoped_ptr<TrayNetworkStateObserver> network_state_observer_;

  DISALLOW_COPY_AND_ASSIGN(TrayNetwork);
};

}  // namespace ash

#endif  // ASH_SYSTEM_CHROMEOS_NETWORK_TRAY_NETWORK_H
