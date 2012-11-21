// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_CHROMEOS_NETWORK_TRAY_VPN_H
#define ASH_SYSTEM_CHROMEOS_NETWORK_TRAY_VPN_H

#include "ash/system/chromeos/network/network_observer.h"
#include "ash/system/tray/system_tray_item.h"
#include "base/memory/scoped_ptr.h"

namespace ash {
namespace internal {

namespace tray {
class NetworkDetailedView;
class VpnDefaultView;
class VpnDetailedView;
}

class TrayVPN : public SystemTrayItem,
                public NetworkObserver {
 public:
  explicit TrayVPN(SystemTray* system_tray);
  virtual ~TrayVPN();

  // Overridden from SystemTrayItem.
  virtual views::View* CreateTrayView(user::LoginStatus status) OVERRIDE;
  virtual views::View* CreateDefaultView(user::LoginStatus status) OVERRIDE;
  virtual views::View* CreateDetailedView(user::LoginStatus status) OVERRIDE;
  virtual views::View* CreateNotificationView(
      user::LoginStatus status) OVERRIDE;
  virtual void DestroyTrayView() OVERRIDE;
  virtual void DestroyDefaultView() OVERRIDE;
  virtual void DestroyDetailedView() OVERRIDE;
  virtual void DestroyNotificationView() OVERRIDE;
  virtual void UpdateAfterLoginStatusChange(user::LoginStatus status) OVERRIDE;
  virtual void UpdateAfterShelfAlignmentChange(
      ShelfAlignment alignment) OVERRIDE;

  // Overridden from NetworkObserver.
  virtual void OnNetworkRefresh(const NetworkIconInfo& info) OVERRIDE;
  virtual void SetNetworkMessage(NetworkTrayDelegate* delegate,
                                 MessageType message_type,
                                 NetworkType network_type,
                                 const string16& title,
                                 const string16& message,
                                 const std::vector<string16>& links) OVERRIDE;
  virtual void ClearNetworkMessage(MessageType message_type) OVERRIDE;
  virtual void OnWillToggleWifi() OVERRIDE;

 private:
  tray::VpnDefaultView* default_;
  tray::NetworkDetailedView* detailed_;

  DISALLOW_COPY_AND_ASSIGN(TrayVPN);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_SYSTEM_CHROMEOS_NETWORK_TRAY_VPN_H
