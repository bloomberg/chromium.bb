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
class DisplayView;

namespace test {
class AshTestBase;
}

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

  static const char kNotificationId[];

  // Scans the current display info and updates |display_info_|. Sets the
  // previous data to |old_info| if it's not NULL.
  void UpdateDisplayInfo(DisplayInfoMap* old_info);

  // Compares the current display settings with |old_info| and determine what
  // message should be shown for notification. Returns true if there's a
  // meaningful change. Note that it's possible to return true and set
  // |message_out| to empty, which means the notification should be removed. It
  // also sets |additional_message_out| which appears in the notification with
  // the |message_out|.
  bool GetDisplayMessageForNotification(
      const DisplayInfoMap& old_info,
      base::string16* message_out,
      base::string16* additional_message_out);

  // Creates or updates the display notification.
  void CreateOrUpdateNotification(const base::string16& message,
                                  const base::string16& additional_message);

  // Overridden from SystemTrayItem.
  virtual views::View* CreateDefaultView(user::LoginStatus status) OVERRIDE;
  virtual void DestroyDefaultView() OVERRIDE;

  // Test accessors.
  base::string16 GetDefaultViewMessage() const;
  bool GetAccessibleStateForTesting(ui::AXViewState* state);
  const views::View* default_view() const {
    return reinterpret_cast<views::View*>(default_);
  }

  DisplayView* default_;
  DisplayInfoMap display_info_;

  DISALLOW_COPY_AND_ASSIGN(TrayDisplay);
};

}  // namespace ash

#endif  // ASH_SYSTEM_CHROMEOS_TRAY_DISPLAY_H_
