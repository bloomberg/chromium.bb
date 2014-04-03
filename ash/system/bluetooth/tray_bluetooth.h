// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_BLUETOOTH_TRAY_BLUETOOTH_H_
#define ASH_SYSTEM_BLUETOOTH_TRAY_BLUETOOTH_H_

#include "ash/system/bluetooth/bluetooth_observer.h"
#include "ash/system/tray/system_tray_item.h"

namespace ash {
namespace tray {
class BluetoothDefaultView;
class BluetoothDetailedView;
}

class TrayBluetooth : public SystemTrayItem,
                      public BluetoothObserver {
 public:
  explicit TrayBluetooth(SystemTray* system_tray);
  virtual ~TrayBluetooth();

 private:
  // Overridden from SystemTrayItem.
  virtual views::View* CreateTrayView(user::LoginStatus status) OVERRIDE;
  virtual views::View* CreateDefaultView(user::LoginStatus status) OVERRIDE;
  virtual views::View* CreateDetailedView(user::LoginStatus status) OVERRIDE;
  virtual void DestroyTrayView() OVERRIDE;
  virtual void DestroyDefaultView() OVERRIDE;
  virtual void DestroyDetailedView() OVERRIDE;
  virtual void UpdateAfterLoginStatusChange(user::LoginStatus status) OVERRIDE;

  // Overridden from BluetoothObserver.
  virtual void OnBluetoothRefresh() OVERRIDE;
  virtual void OnBluetoothDiscoveringChanged() OVERRIDE;

  tray::BluetoothDefaultView* default_;
  tray::BluetoothDetailedView* detailed_;

  DISALLOW_COPY_AND_ASSIGN(TrayBluetooth);
};

}  // namespace ash

#endif  // ASH_SYSTEM_BLUETOOTH_TRAY_BLUETOOTH_H_
