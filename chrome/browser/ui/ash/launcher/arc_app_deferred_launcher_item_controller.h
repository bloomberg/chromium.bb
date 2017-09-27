// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LAUNCHER_ARC_APP_DEFERRED_LAUNCHER_ITEM_CONTROLLER_H_
#define CHROME_BROWSER_UI_ASH_LAUNCHER_ARC_APP_DEFERRED_LAUNCHER_ITEM_CONTROLLER_H_

#include <stdint.h>

#include <string>

#include "ash/public/cpp/shelf_item_delegate.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"

class ArcAppDeferredLauncherController;

// ArcAppDeferredLauncherItemController displays the icon of the ARC app that
// cannot be launched immediately (due to ARC not being ready) on Chrome OS'
// shelf, with an overlaid spinner to provide visual feedback.
class ArcAppDeferredLauncherItemController : public ash::ShelfItemDelegate {
 public:
  ArcAppDeferredLauncherItemController(
      const std::string& arc_app_id,
      int event_flags,
      int64_t display_id,
      const base::WeakPtr<ArcAppDeferredLauncherController>& host);

  ~ArcAppDeferredLauncherItemController() override;

  base::TimeDelta GetActiveTime() const;

  int event_flags() const { return event_flags_; }
  int64_t display_id() const { return display_id_; }

  // ash::ShelfItemDelegate:
  void ItemSelected(std::unique_ptr<ui::Event> event,
                    int64_t display_id,
                    ash::ShelfLaunchSource source,
                    ItemSelectedCallback callback) override;
  void ExecuteCommand(bool from_context_menu,
                      int64_t command_id,
                      int32_t event_flags,
                      int64_t display_id) override;
  void Close() override;

 private:
  // The flags of the event that caused the ARC app to be activated. These will
  // be propagated to the launch event once the app is actually launched.
  const int event_flags_;

  const int64_t display_id_;

  base::WeakPtr<ArcAppDeferredLauncherController> host_;
  const base::Time start_time_;

  DISALLOW_COPY_AND_ASSIGN(ArcAppDeferredLauncherItemController);
};

#endif  // CHROME_BROWSER_UI_ASH_LAUNCHER_ARC_APP_DEFERRED_LAUNCHER_ITEM_CONTROLLER_H_
