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
  ~TrayBluetooth() override;

 private:
  // Overridden from SystemTrayItem.
  views::View* CreateTrayView(user::LoginStatus status) override;
  views::View* CreateDefaultView(user::LoginStatus status) override;
  views::View* CreateDetailedView(user::LoginStatus status) override;
  void DestroyTrayView() override;
  void DestroyDefaultView() override;
  void DestroyDetailedView() override;
  void UpdateAfterLoginStatusChange(user::LoginStatus status) override;

  // Overridden from BluetoothObserver.
  void OnBluetoothRefresh() override;
  void OnBluetoothDiscoveringChanged() override;

  tray::BluetoothDefaultView* default_;
  tray::BluetoothDetailedView* detailed_;

  DISALLOW_COPY_AND_ASSIGN(TrayBluetooth);
};

}  // namespace ash

#endif  // ASH_SYSTEM_BLUETOOTH_TRAY_BLUETOOTH_H_
