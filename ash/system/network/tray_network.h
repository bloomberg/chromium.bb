// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NETWORK_TRAY_NETWORK_H_
#define ASH_SYSTEM_NETWORK_TRAY_NETWORK_H_

#include <memory>
#include <set>

#include "ash/system/network/network_observer.h"
#include "ash/system/network/network_portal_detector_observer.h"
#include "ash/system/network/tray_network_state_observer.h"
#include "ash/system/tray/system_tray_item.h"
#include "base/macros.h"
#include "base/time/time.h"

namespace ash {
namespace tray {
class NetworkDefaultView;
class NetworkListView;
class NetworkTrayView;
}

class TrayNetwork : public SystemTrayItem,
                    public NetworkObserver,
                    public NetworkPortalDetectorObserver,
                    public TrayNetworkStateObserver::Delegate {
 public:
  explicit TrayNetwork(SystemTray* system_tray);
  ~TrayNetwork() override;

  tray::NetworkListView* detailed() { return detailed_; }

  // SystemTrayItem
  views::View* CreateTrayView(LoginStatus status) override;
  views::View* CreateDefaultView(LoginStatus status) override;
  views::View* CreateDetailedView(LoginStatus status) override;
  void DestroyTrayView() override;
  void DestroyDefaultView() override;
  void DestroyDetailedView() override;

  // NetworkObserver
  void RequestToggleWifi() override;

  // NetworkPortalDetectorObserver
  void OnCaptivePortalDetected(const std::string& guid) override;

  // TrayNetworkStateObserver::Delegate
  void NetworkStateChanged() override;

 private:
  tray::NetworkTrayView* tray_;
  tray::NetworkDefaultView* default_;
  tray::NetworkListView* detailed_;
  std::unique_ptr<TrayNetworkStateObserver> network_state_observer_;

  DISALLOW_COPY_AND_ASSIGN(TrayNetwork);
};

}  // namespace ash

#endif  // ASH_SYSTEM_NETWORK_TRAY_NETWORK_H_
