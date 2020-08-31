// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_BLUETOOTH_BLUETOOTH_FEATURE_POD_CONTROLLER_H_
#define ASH_SYSTEM_BLUETOOTH_BLUETOOTH_FEATURE_POD_CONTROLLER_H_

#include "ash/system/bluetooth/tray_bluetooth_helper.h"
#include "ash/system/unified/feature_pod_controller_base.h"
#include "base/macros.h"
#include "base/strings/string16.h"

namespace ash {

class UnifiedSystemTrayController;

// Controller of a feature pod button of bluetooth.
class BluetoothFeaturePodController : public FeaturePodControllerBase,
                                      public TrayBluetoothHelper::Observer {
 public:
  BluetoothFeaturePodController(UnifiedSystemTrayController* tray_controller);
  ~BluetoothFeaturePodController() override;

  // FeaturePodControllerBase:
  FeaturePodButton* CreateButton() override;
  void OnIconPressed() override;
  void OnLabelPressed() override;
  SystemTrayItemUmaType GetUmaType() const override;

 private:
  void UpdateButton();
  void SetTooltipState(const base::string16& tooltip_state);

  // BluetoothObserver:
  void OnBluetoothSystemStateChanged() override;
  void OnBluetoothScanStateChanged() override;
  void OnBluetoothDeviceListChanged() override;

  // Unowned.
  UnifiedSystemTrayController* const tray_controller_;
  FeaturePodButton* button_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(BluetoothFeaturePodController);
};

}  // namespace ash

#endif  // ASH_SYSTEM_BLUETOOTH_BLUETOOTH_FEATURE_POD_CONTROLLER_H_
