// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SYSTEM_UPDATE_TRAY_UPDATE_H_
#define ASH_COMMON_SYSTEM_UPDATE_TRAY_UPDATE_H_

#include "ash/ash_export.h"
#include "ash/common/system/tray/tray_image_item.h"
#include "ash/common/system/update/update_observer.h"
#include "base/macros.h"

namespace views {
class View;
}

namespace ash {

// The system update tray item. Exported for test.
class ASH_EXPORT TrayUpdate : public TrayImageItem, public UpdateObserver {
 public:
  explicit TrayUpdate(SystemTray* system_tray);
  ~TrayUpdate() override;

 private:
  // Overridden from TrayImageItem.
  bool GetInitialVisibility() override;
  views::View* CreateDefaultView(LoginStatus status) override;

  // Overridden from UpdateObserver.
  void OnUpdateRecommended(const UpdateInfo& info) override;

  DISALLOW_COPY_AND_ASSIGN(TrayUpdate);
};

}  // namespace ash

#endif  // ASH_COMMON_SYSTEM_UPDATE_TRAY_UPDATE_H_
