// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_CHROMEOS_TRAY_DISPLAY_H_
#define ASH_SYSTEM_CHROMEOS_TRAY_DISPLAY_H_

#include <stdint.h>

#include <map>

#include "ash/ash_export.h"
#include "ash/common/display/display_info.h"
#include "ash/common/system/tray/system_tray_item.h"
#include "ash/common/wm_display_observer.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "ui/views/view.h"

namespace ash {

class DisplayView;

class ASH_EXPORT TrayDisplay : public SystemTrayItem, public WmDisplayObserver {
 public:
  explicit TrayDisplay(SystemTray* system_tray);
  ~TrayDisplay() override;

  // Overridden from WmDisplayObserver:
  void OnDisplayConfigurationChanged() override;

 private:
  friend class TrayDisplayTest;

  typedef std::map<int64_t, DisplayInfo> DisplayInfoMap;

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
  bool GetDisplayMessageForNotification(const DisplayInfoMap& old_info,
                                        base::string16* message_out,
                                        base::string16* additional_message_out);

  // Creates or updates the display notification.
  void CreateOrUpdateNotification(const base::string16& message,
                                  const base::string16& additional_message);

  // Overridden from SystemTrayItem.
  views::View* CreateDefaultView(LoginStatus status) override;
  void DestroyDefaultView() override;

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
