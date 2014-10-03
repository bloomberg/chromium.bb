// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_CHROMEOS_SETTINGS_TRAY_SETTINGS_H_
#define ASH_SYSTEM_CHROMEOS_SETTINGS_TRAY_SETTINGS_H_

#include "ash/system/tray/system_tray_item.h"

namespace ash {
namespace tray {
class SettingsDefaultView;
}

class TraySettings : public SystemTrayItem {
 public:
  explicit TraySettings(SystemTray* system_tray);
  virtual ~TraySettings();

 private:
  // Overridden from SystemTrayItem
  virtual views::View* CreateTrayView(user::LoginStatus status) override;
  virtual views::View* CreateDefaultView(user::LoginStatus status) override;
  virtual views::View* CreateDetailedView(user::LoginStatus status) override;
  virtual void DestroyTrayView() override;
  virtual void DestroyDefaultView() override;
  virtual void DestroyDetailedView() override;
  virtual void UpdateAfterLoginStatusChange(user::LoginStatus status) override;

  tray::SettingsDefaultView* default_view_;

  DISALLOW_COPY_AND_ASSIGN(TraySettings);
};

}  // namespace ash

#endif  // ASH_SYSTEM_CHROMEOS_SETTINGS_TRAY_SETTINGS_H_
