// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NETWORK_TRAY_NETWORK_H
#define ASH_SYSTEM_NETWORK_TRAY_NETWORK_H
#pragma once

#include "ash/system/network/network_observer.h"
#include "ash/system/tray/system_tray_item.h"

namespace ash {
namespace internal {

namespace tray {
class NetworkDefaultView;
class NetworkDetailedView;
class NetworkTrayView;
}

class TrayNetwork : public SystemTrayItem,
                    public NetworkObserver {
 public:
  TrayNetwork();
  virtual ~TrayNetwork();

 private:
  // Overridden from SystemTrayItem.
  virtual views::View* CreateTrayView(user::LoginStatus status) OVERRIDE;
  virtual views::View* CreateDefaultView(user::LoginStatus status) OVERRIDE;
  virtual views::View* CreateDetailedView(user::LoginStatus status) OVERRIDE;
  virtual void DestroyTrayView() OVERRIDE;
  virtual void DestroyDefaultView() OVERRIDE;
  virtual void DestroyDetailedView() OVERRIDE;
  virtual void UpdateAfterLoginStatusChange(user::LoginStatus status) OVERRIDE;

  // Overridden from NetworkObserver.
  virtual void OnNetworkRefresh(const NetworkIconInfo& info) OVERRIDE;

  tray::NetworkTrayView* tray_;
  tray::NetworkDefaultView* default_;
  tray::NetworkDetailedView* detailed_;

  DISALLOW_COPY_AND_ASSIGN(TrayNetwork);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_SYSTEM_NETWORK_TRAY_NETWORK_H
