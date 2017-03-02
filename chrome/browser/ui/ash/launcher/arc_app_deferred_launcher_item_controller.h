// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LAUNCHER_ARC_APP_DEFERRED_LAUNCHER_ITEM_CONTROLLER_H_
#define CHROME_BROWSER_UI_ASH_LAUNCHER_ARC_APP_DEFERRED_LAUNCHER_ITEM_CONTROLLER_H_

#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ui/ash/launcher/launcher_item_controller.h"

class ArcAppDeferredLauncherController;
class ChromeLauncherController;

// ArcAppDeferredLauncherItemController displays the icon of the ARC app that
// cannot be launched immediately (due to ARC not being ready) on Chrome OS'
// shelf, with an overlaid spinner to provide visual feedback.
class ArcAppDeferredLauncherItemController : public LauncherItemController {
 public:
  ArcAppDeferredLauncherItemController(
      const std::string& arc_app_id,
      ChromeLauncherController* controller,
      int event_flags,
      const base::WeakPtr<ArcAppDeferredLauncherController>& host);

  ~ArcAppDeferredLauncherItemController() override;

  base::TimeDelta GetActiveTime() const;

  int event_flags() const { return event_flags_; }

  // ash::ShelfItemDelegate
  ash::ShelfAction ItemSelected(ui::EventType event_type,
                                int event_flags,
                                int64_t display_id,
                                ash::ShelfLaunchSource source) override;
  ash::ShelfAppMenuItemList GetAppMenuItems(int event_flags) override;
  void ExecuteCommand(uint32_t command_id, int event_flags) override;
  void Close() override;

 private:
  // The flags of the event that caused the ARC app to be activated. These will
  // be propagated to the launch event once the app is actually launched.
  const int event_flags_;

  base::WeakPtr<ArcAppDeferredLauncherController> host_;
  const base::Time start_time_;

  DISALLOW_COPY_AND_ASSIGN(ArcAppDeferredLauncherItemController);
};

#endif  // CHROME_BROWSER_UI_ASH_LAUNCHER_ARC_APP_DEFERRED_LAUNCHER_ITEM_CONTROLLER_H_
