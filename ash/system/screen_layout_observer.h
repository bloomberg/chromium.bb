// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_SCREEN_LAYOUT_OBSERVER_H_
#define ASH_SYSTEM_SCREEN_LAYOUT_OBSERVER_H_

#include <stdint.h>

#include <map>

#include "ash/ash_export.h"
#include "ash/wm_display_observer.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "ui/display/manager/managed_display_info.h"

namespace ash {

// ScreenLayoutObserver is responsible to send notification to users when screen
// resolution changes or screen rotation changes.
class ASH_EXPORT ScreenLayoutObserver : public WmDisplayObserver {
 public:
  ScreenLayoutObserver();
  ~ScreenLayoutObserver() override;

  // Overridden from WmDisplayObserver:
  void OnDisplayConfigurationChanged() override;

  // Notifications are shown in production and are not shown in unit tests.
  // Allow individual unit tests to show notifications.
  void set_show_notifications_for_testing(bool show) {
    show_notifications_for_testing_ = show;
  }

 private:
  friend class ScreenLayoutObserverTest;

  using DisplayInfoMap = std::map<int64_t, display::ManagedDisplayInfo>;

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
                                        base::string16* out_message,
                                        base::string16* out_additional_message);

  // Creates or updates the display notification.
  void CreateOrUpdateNotification(const base::string16& message,
                                  const base::string16& additional_message);

  // Returns the notification message that should be shown when mirror display
  // mode is exited.
  bool GetExitMirrorModeMessage(base::string16* out_message,
                                base::string16* out_additional_message);

  DisplayInfoMap display_info_;

  enum class DisplayMode {
    SINGLE,
    EXTENDED_2,       // 2 displays in extended mode.
    EXTENDED_3_PLUS,  // 3+ displays in extended mode.
    MIRRORING,
    UNIFIED,
    DOCKED
  };

  DisplayMode old_display_mode_ = DisplayMode::SINGLE;
  DisplayMode current_display_mode_ = DisplayMode::SINGLE;

  bool show_notifications_for_testing_ = true;

  DISALLOW_COPY_AND_ASSIGN(ScreenLayoutObserver);
};

}  // namespace ash

#endif  // ASH_SYSTEM_SCREEN_LAYOUT_OBSERVER_H_
