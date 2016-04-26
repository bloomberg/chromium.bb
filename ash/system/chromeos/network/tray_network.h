// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_CHROMEOS_NETWORK_TRAY_NETWORK_H
#define ASH_SYSTEM_CHROMEOS_NETWORK_TRAY_NETWORK_H

#include <memory>
#include <set>

#include "ash/system/chromeos/network/network_observer.h"
#include "ash/system/chromeos/network/network_portal_detector_observer.h"
#include "ash/system/chromeos/network/tray_network_state_observer.h"
#include "ash/system/tray/system_tray_item.h"
#include "base/macros.h"
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
  ~TrayNetwork() override;

  tray::NetworkDetailedView* detailed() { return detailed_; }

  // SystemTrayItem
  views::View* CreateTrayView(user::LoginStatus status) override;
  views::View* CreateDefaultView(user::LoginStatus status) override;
  views::View* CreateDetailedView(user::LoginStatus status) override;
  void DestroyTrayView() override;
  void DestroyDefaultView() override;
  void DestroyDetailedView() override;
  void UpdateAfterLoginStatusChange(user::LoginStatus status) override;
  void UpdateAfterShelfAlignmentChange(wm::ShelfAlignment alignment) override;

  // NetworkObserver
  void RequestToggleWifi() override;

  // NetworkPortalDetectorObserver
  void OnCaptivePortalDetected(const std::string& service_path) override;

  // TrayNetworkStateObserver::Delegate
  void NetworkStateChanged() override;

 private:
  tray::NetworkTrayView* tray_;
  tray::NetworkDefaultView* default_;
  tray::NetworkDetailedView* detailed_;
  bool request_wifi_view_;
  std::unique_ptr<TrayNetworkStateObserver> network_state_observer_;

  DISALLOW_COPY_AND_ASSIGN(TrayNetwork);
};

}  // namespace ash

#endif  // ASH_SYSTEM_CHROMEOS_NETWORK_TRAY_NETWORK_H
