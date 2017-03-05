// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SYSTEM_CHROMEOS_BLUETOOTH_TRAY_BLUETOOTH_H_
#define ASH_COMMON_SYSTEM_CHROMEOS_BLUETOOTH_TRAY_BLUETOOTH_H_

#include "ash/common/system/chromeos/bluetooth/bluetooth_observer.h"
#include "ash/common/system/tray/system_tray_item.h"
#include "base/macros.h"

namespace ash {
namespace tray {
class BluetoothDefaultView;
class BluetoothDetailedView;
}

class TrayBluetooth : public SystemTrayItem, public BluetoothObserver {
 public:
  explicit TrayBluetooth(SystemTray* system_tray);
  ~TrayBluetooth() override;

 private:
  // Overridden from SystemTrayItem.
  views::View* CreateTrayView(LoginStatus status) override;
  views::View* CreateDefaultView(LoginStatus status) override;
  views::View* CreateDetailedView(LoginStatus status) override;
  void DestroyTrayView() override;
  void DestroyDefaultView() override;
  void DestroyDetailedView() override;
  void UpdateAfterLoginStatusChange(LoginStatus status) override;

  // Overridden from BluetoothObserver.
  void OnBluetoothRefresh() override;
  void OnBluetoothDiscoveringChanged() override;

  tray::BluetoothDefaultView* default_;
  tray::BluetoothDetailedView* detailed_;

  DISALLOW_COPY_AND_ASSIGN(TrayBluetooth);
};

}  // namespace ash

#endif  // ASH_COMMON_SYSTEM_CHROMEOS_BLUETOOTH_TRAY_BLUETOOTH_H_
