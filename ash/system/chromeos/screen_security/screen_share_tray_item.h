// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_CHROMEOS_SCREEN_SHARE_SCREEN_SHARE_TRAY_ITEM_H_
#define ASH_SYSTEM_CHROMEOS_SCREEN_SHARE_SCREEN_SHARE_TRAY_ITEM_H_

#include "ash/system/chromeos/screen_security/screen_share_observer.h"
#include "ash/system/chromeos/screen_security/screen_tray_item.h"

namespace views {
class View;
}

namespace ash {

class ASH_EXPORT ScreenShareTrayItem : public ScreenTrayItem,
                                       public ScreenShareObserver {
 public:
  explicit ScreenShareTrayItem(SystemTray* system_tray);
  ~ScreenShareTrayItem() override;

 private:
  // Overridden from SystemTrayItem.
  views::View* CreateTrayView(user::LoginStatus status) override;
  views::View* CreateDefaultView(user::LoginStatus status) override;

  // Overridden from ScreenTrayItem.
  void CreateOrUpdateNotification() override;
  std::string GetNotificationId() override;

  // Overridden from ScreenShareObserver.
  void OnScreenShareStart(const base::Closure& stop_callback,
                          const base::string16& helper_name) override;
  void OnScreenShareStop() override;

  base::string16 helper_name_;

  DISALLOW_COPY_AND_ASSIGN(ScreenShareTrayItem);
};

}  // namespace ash

#endif  // ASH_SYSTEM_CHROMEOS_SCREEN_SHARE_SCREEN_SHARE_TRAY_ITEM_H_
