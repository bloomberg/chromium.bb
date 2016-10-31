// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SYSTEM_CHROMEOS_SETTINGS_TRAY_SETTINGS_H_
#define ASH_COMMON_SYSTEM_CHROMEOS_SETTINGS_TRAY_SETTINGS_H_

#include "ash/common/system/tray/system_tray_item.h"
#include "base/macros.h"

namespace ash {
namespace tray {
class SettingsDefaultView;
}

// TODO(tdanderson): Remove this class once material design is enabled by
// default. See crbug.com/614453.
class TraySettings : public SystemTrayItem {
 public:
  explicit TraySettings(SystemTray* system_tray);
  ~TraySettings() override;

 private:
  // Overridden from SystemTrayItem
  views::View* CreateTrayView(LoginStatus status) override;
  views::View* CreateDefaultView(LoginStatus status) override;
  views::View* CreateDetailedView(LoginStatus status) override;
  void DestroyTrayView() override;
  void DestroyDefaultView() override;
  void DestroyDetailedView() override;
  void UpdateAfterLoginStatusChange(LoginStatus status) override;

  tray::SettingsDefaultView* default_view_;

  DISALLOW_COPY_AND_ASSIGN(TraySettings);
};

}  // namespace ash

#endif  // ASH_COMMON_SYSTEM_CHROMEOS_SETTINGS_TRAY_SETTINGS_H_
