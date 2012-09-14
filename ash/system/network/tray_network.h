// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NETWORK_TRAY_NETWORK_H
#define ASH_SYSTEM_NETWORK_TRAY_NETWORK_H

#include "ash/system/network/network_observer.h"
#include "ash/system/tray/system_tray_item.h"
#include "base/memory/scoped_ptr.h"

namespace ash {
namespace internal {

namespace tray {
class NetworkDefaultView;
class NetworkDetailedView;
class NetworkMessages;
class NetworkMessageView;
class NetworkNotificationView;
class NetworkTrayView;
}

class TrayNetwork : public SystemTrayItem,
                    public NetworkObserver {
 public:
  enum DetailedViewType {
    LIST_VIEW,
    WIFI_VIEW,
  };

  TrayNetwork();
  virtual ~TrayNetwork();

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
                                 const string16& title,
                                 const string16& message,
                                 const std::vector<string16>& links) OVERRIDE;
  virtual void ClearNetworkMessage(MessageType message_type) OVERRIDE;
  virtual void OnWillToggleWifi() OVERRIDE;

 private:
  friend class tray::NetworkMessageView;
  friend class tray::NetworkNotificationView;

  void LinkClicked(MessageType message_type, int link_id);

  const tray::NetworkMessages* messages() const { return messages_.get(); }

  tray::NetworkTrayView* tray_;
  tray::NetworkDefaultView* default_;
  tray::NetworkDetailedView* detailed_;
  tray::NetworkNotificationView* notification_;
  scoped_ptr<tray::NetworkMessages> messages_;
  bool request_wifi_view_;

  DISALLOW_COPY_AND_ASSIGN(TrayNetwork);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_SYSTEM_NETWORK_TRAY_NETWORK_H
