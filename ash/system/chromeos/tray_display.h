// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_CHROMEOS_TRAY_DISPLAY_H_
#define ASH_SYSTEM_CHROMEOS_TRAY_DISPLAY_H_

#include <map>

#include "ash/ash_export.h"
#include "ash/display/display_controller.h"
#include "ash/display/display_info.h"
#include "ash/system/tray/system_tray_item.h"
#include "base/strings/string16.h"
#include "ui/views/view.h"

namespace ash {
namespace test {
class AshTestBase;
}

namespace internal {

class DisplayNotificationView;

class ASH_EXPORT TrayDisplay : public SystemTrayItem,
                               public DisplayController::Observer {
 public:
  explicit TrayDisplay(SystemTray* system_tray);
  virtual ~TrayDisplay();

  // Overridden from DisplayControllerObserver:
  virtual void OnDisplayConfigurationChanged() OVERRIDE;

 private:
  friend class TrayDisplayTest;

  typedef std::map<int64, DisplayInfo> DisplayInfoMap;

  // Scans the current display info and updates |display_info_|. Sets the
  // previous data to |old_info| if it's not NULL.
  void UpdateDisplayInfo(DisplayInfoMap* old_info);

  // Compares the current display settings with |old_info| and determine what
  // message should be shown for notification. Returns true if there's a
  // meaningful change. Note that it's possible to return true and set |message|
  // to empty, which means the notification should be removed.
  bool GetDisplayMessageForNotification(
      base::string16* message,
      const DisplayInfoMap& old_info);

  // Overridden from SystemTrayItem.
  virtual views::View* CreateDefaultView(user::LoginStatus status) OVERRIDE;
  virtual void DestroyDefaultView() OVERRIDE;

  // Test accessors.
  base::string16 GetDefaultViewMessage();
  base::string16 GetNotificationMessage();
  void CloseNotificationForTest();
  views::View* default_view() { return default_; }

  views::View* default_;
  DisplayInfoMap display_info_;

  DISALLOW_COPY_AND_ASSIGN(TrayDisplay);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_SYSTEM_CHROMEOS_TRAY_DISPLAY_H_
