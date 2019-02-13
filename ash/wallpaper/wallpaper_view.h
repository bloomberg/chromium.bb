// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WALLPAPER_WALLPAPER_VIEW_H_
#define ASH_WALLPAPER_WALLPAPER_VIEW_H_

#include <memory>

#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_observer.h"
#include "base/scoped_observer.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/view.h"

namespace aura {
class Window;
}

namespace ash {

class WallpaperView : public views::View,
                      public views::ContextMenuController,
                      TabletModeObserver {
 public:
  WallpaperView();
  ~WallpaperView() override;

 private:
  friend class WallpaperControllerTest;

  // Overridden from views::View:
  void OnPaint(gfx::Canvas* canvas) override;
  bool OnMousePressed(const ui::MouseEvent& event) override;

  // Overridden from TabletModeObserver:
  void OnTabletModeStarted() override;
  void OnTabletModeEnded() override;
  void OnTabletControllerDestroyed() override;

  // Overridden from views::ContextMenuController:
  void ShowContextMenuForView(views::View* source,
                              const gfx::Point& point,
                              ui::MenuSourceType source_type) override;

  ScopedObserver<TabletModeController, TabletModeObserver>
      tablet_mode_observer_{this};
  bool is_tablet_mode_ = false;

  DISALLOW_COPY_AND_ASSIGN(WallpaperView);
};

views::Widget* CreateWallpaperWidget(aura::Window* root_window,
                                     int container_id);

}  // namespace ash

#endif  // ASH_WALLPAPER_WALLPAPER_VIEW_H_
