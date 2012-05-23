// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NETWORK_TRAY_NETWORK_H
#define ASH_SYSTEM_NETWORK_TRAY_NETWORK_H
#pragma once

#include "ash/system/network/network_observer.h"
#include "ash/system/tray/system_tray_item.h"
#include "base/memory/scoped_ptr.h"

namespace ash {
namespace internal {

namespace tray {
class NetworkDefaultView;
class NetworkDetailedView;
class NetworkErrors;
class NetworkErrorView;
class NetworkNotificationView;
class NetworkTrayView;
}

class TrayNetwork : public SystemTrayItem,
                    public NetworkObserver {
 public:
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

  // Overridden from NetworkObserver.
  virtual void OnNetworkRefresh(const NetworkIconInfo& info) OVERRIDE;
  virtual void SetNetworkError(NetworkTrayDelegate* delegate,
                               ErrorType error_type,
                               const string16& title,
                               const string16& message,
                               const string16& link_text) OVERRIDE;
  virtual void ClearNetworkError(ErrorType error_type) OVERRIDE;

 private:
  friend class tray::NetworkErrorView;
  friend class tray::NetworkNotificationView;

  void LinkClicked(ErrorType error_type);

  const tray::NetworkErrors* errors() const { return errors_.get(); }

  tray::NetworkTrayView* tray_;
  tray::NetworkDefaultView* default_;
  tray::NetworkDetailedView* detailed_;
  tray::NetworkNotificationView* notification_;
  scoped_ptr<tray::NetworkErrors> errors_;

  DISALLOW_COPY_AND_ASSIGN(TrayNetwork);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_SYSTEM_NETWORK_TRAY_NETWORK_H
