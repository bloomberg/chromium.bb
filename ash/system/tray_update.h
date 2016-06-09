// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_UPDATE_H_
#define ASH_SYSTEM_TRAY_UPDATE_H_

#include "ash/system/tray/tray_image_item.h"
#include "ash/system/user/update_observer.h"
#include "base/macros.h"

namespace views {
class View;
}

namespace ash {

class TrayUpdate : public TrayImageItem,
                   public UpdateObserver {
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

#endif  // ASH_SYSTEM_TRAY_UPDATE_H_
