// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SYSTEM_UPDATE_TRAY_UPDATE_H_
#define ASH_COMMON_SYSTEM_UPDATE_TRAY_UPDATE_H_

#include "ash/ash_export.h"
#include "ash/common/system/tray/tray_image_item.h"
#include "base/macros.h"

namespace views {
class View;
}

namespace ash {

namespace mojom {
enum class UpdateSeverity;
}

// The system update tray item. The tray icon stays visible once an update
// notification is received. The icon only disappears after a reboot to apply
// the update. Exported for test.
class ASH_EXPORT TrayUpdate : public TrayImageItem {
 public:
  explicit TrayUpdate(SystemTray* system_tray);
  ~TrayUpdate() override;

  // Shows an icon in the system tray indicating that a software update is
  // available. Once shown the icon persists until reboot. |severity| and
  // |factory_reset_required| are used to set the icon, color, and tooltip.
  void ShowUpdateIcon(mojom::UpdateSeverity severity,
                      bool factory_reset_required);

 private:
  class UpdateView;

  // Overridden from TrayImageItem.
  bool GetInitialVisibility() override;
  views::View* CreateDefaultView(LoginStatus status) override;

  // If an external monitor is connected then the system tray may be created
  // after the update data is sent from chrome, so share the update info between
  // all instances.
  static bool update_required_;
  static mojom::UpdateSeverity severity_;
  static bool factory_reset_required_;

  DISALLOW_COPY_AND_ASSIGN(TrayUpdate);
};

}  // namespace ash

#endif  // ASH_COMMON_SYSTEM_UPDATE_TRAY_UPDATE_H_
