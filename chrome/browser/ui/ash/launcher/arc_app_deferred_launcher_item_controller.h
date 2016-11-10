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

class ArcAppDeferredLauncherItemController : public LauncherItemController {
 public:
  ArcAppDeferredLauncherItemController(
      const std::string& arc_app_id,
      ChromeLauncherController* controller,
      const base::WeakPtr<ArcAppDeferredLauncherController>& host);

  ~ArcAppDeferredLauncherItemController() override;

  base::TimeDelta GetActiveTime() const;

  // ash::ShelfItemDelegate
  ash::ShelfItemDelegate::PerformedAction ItemSelected(
      const ui::Event& event) override;
  base::string16 GetTitle() override;
  bool CanPin() const override;
  ash::ShelfMenuModel* CreateApplicationMenu(int event_flags) override;
  bool IsDraggable() override;
  bool ShouldShowTooltip() override;
  void Close() override;

  // LauncherItemController overrides:
  bool IsVisible() const override;
  void Launch(ash::LaunchSource source, int event_flags) override;
  ash::ShelfItemDelegate::PerformedAction Activate(
      ash::LaunchSource source) override;
  ChromeLauncherAppMenuItems GetApplicationList(int event_flags) override;

 private:
  base::WeakPtr<ArcAppDeferredLauncherController> host_;
  const base::Time start_time_;

  DISALLOW_COPY_AND_ASSIGN(ArcAppDeferredLauncherItemController);
};

#endif  // CHROME_BROWSER_UI_ASH_LAUNCHER_ARC_APP_DEFERRED_LAUNCHER_ITEM_CONTROLLER_H_
