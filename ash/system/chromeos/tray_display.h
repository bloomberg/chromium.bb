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

 private:
  friend class TrayDisplayTest;

  typedef std::map<int64, DisplayInfo> DisplayInfoMap;

  // Checks the current display settings and determine what message should be
  // shown for notification.
  base::string16 GetDisplayMessageForNotification();

  // Overridden from SystemTrayItem.
  virtual views::View* CreateDefaultView(user::LoginStatus status) OVERRIDE;
  virtual views::View* CreateNotificationView(
      user::LoginStatus status) OVERRIDE;
  virtual void DestroyDefaultView() OVERRIDE;
  virtual void DestroyNotificationView() OVERRIDE;
  virtual bool ShouldShowLauncher() const OVERRIDE;

  // Overridden from DisplayControllerObserver:
  virtual void OnDisplayConfigurationChanged() OVERRIDE;

  // Test accessors.
  base::string16 GetDefaultViewMessage();
  views::View* default_view() { return default_; }
  const string16& current_message() const { return current_message_; }

  views::View* default_;
  DisplayNotificationView* notification_;
  string16 current_message_;
  DisplayInfoMap display_info_;

  DISALLOW_COPY_AND_ASSIGN(TrayDisplay);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_SYSTEM_CHROMEOS_TRAY_DISPLAY_H_
