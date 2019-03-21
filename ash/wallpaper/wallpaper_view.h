// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WALLPAPER_WALLPAPER_VIEW_H_
#define ASH_WALLPAPER_WALLPAPER_VIEW_H_

#include "ui/views/context_menu_controller.h"
#include "ui/views/view.h"

namespace aura {
class Window;
}

namespace ash {

class WallpaperView : public views::View, public views::ContextMenuController {
 public:
  WallpaperView();
  ~WallpaperView() override;

  // Schedules a repaint of the wallpaper with blur and opacity changes.
  void RepaintBlurAndOpacity(int repaint_blur, float repaint_opacity);

 private:
  friend class WallpaperControllerTest;

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;
  bool OnMousePressed(const ui::MouseEvent& event) override;

  // views::ContextMenuController:
  void ShowContextMenuForViewImpl(views::View* source,
                                  const gfx::Point& point,
                                  ui::MenuSourceType source_type) override;

  // Helper to draw the wallpaper.
  void DrawWallpaper(const gfx::ImageSkia& wallpaper,
                     const gfx::Rect& src,
                     const gfx::Rect& dst,
                     const cc::PaintFlags& flags,
                     gfx::Canvas* canvas);

  // These are used by overview mode to animate the blur and opacity on the
  // wallpaper. If |repaint_blur_| is not 0 and |repaint_opacity_| is not 1, the
  // wallpaper will be downsampled and a blur and brightness filter will be
  // applied. It is downsampled to increase performance.
  int repaint_blur_ = 0;
  float repaint_opacity_ = 1.f;

  DISALLOW_COPY_AND_ASSIGN(WallpaperView);
};

views::Widget* CreateWallpaperWidget(aura::Window* root_window,
                                     int container_id,
                                     WallpaperView** out_wallpaper_view);

}  // namespace ash

#endif  // ASH_WALLPAPER_WALLPAPER_VIEW_H_
