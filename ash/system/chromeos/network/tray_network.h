// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_CHROMEOS_NETWORK_TRAY_NETWORK_H
#define ASH_SYSTEM_CHROMEOS_NETWORK_TRAY_NETWORK_H

#include <set>

#include "ash/system/chromeos/network/network_icon.h"
#include "ash/system/chromeos/network/network_observer.h"
#include "ash/system/chromeos/network/tray_network_state_observer.h"
#include "ash/system/tray/system_tray_item.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"

namespace chromeos {
class NetworkState;
}

namespace ash {
namespace internal {

class NetworkStateNotifier;
class TrayNetworkStateObserver;

namespace tray {
class NetworkDefaultView;
class NetworkDetailedView;
class NetworkMessages;
class NetworkMessageView;
class NetworkNotificationView;
class NetworkTrayView;
}

class TrayNetwork : public SystemTrayItem,
                    public NetworkObserver,
                    public TrayNetworkStateObserver::Delegate {
 public:
  explicit TrayNetwork(SystemTray* system_tray);
  virtual ~TrayNetwork();

  tray::NetworkDetailedView* detailed() { return detailed_; }
  void set_request_wifi_view(bool b) { request_wifi_view_ = b; }

  // SystemTrayItem
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

  // NetworkObserver
  virtual void OnNetworkRefresh(const NetworkIconInfo& info) OVERRIDE;
  virtual void SetNetworkMessage(
      NetworkTrayDelegate* delegate,
      MessageType message_type,
      NetworkType network_type,
      const base::string16& title,
      const base::string16& message,
      const std::vector<base::string16>& links) OVERRIDE;
  virtual void ClearNetworkMessage(MessageType message_type) OVERRIDE;
  virtual void OnWillToggleWifi() OVERRIDE;

  // TrayNetworkStateObserver::Delegate
  virtual void NetworkStateChanged(bool list_changed) OVERRIDE;
  virtual void NetworkServiceChanged(
      const chromeos::NetworkState* network) OVERRIDE;

  // Gets the correct icon and label for |icon_type|. Also sets |animating|
  // based on whether or not the icon is animating (i.e. connecting).
  void GetNetworkStateHandlerImageAndLabel(network_icon::IconType icon_type,
                                           gfx::ImageSkia* image,
                                           base::string16* label,
                                           bool* animating);

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
  scoped_ptr<TrayNetworkStateObserver> network_state_observer_;
  scoped_ptr<NetworkStateNotifier> network_state_notifier_;

  DISALLOW_COPY_AND_ASSIGN(TrayNetwork);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_SYSTEM_CHROMEOS_NETWORK_TRAY_NETWORK_H
