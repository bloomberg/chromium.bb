// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AURA_WM_ROOT_CONTROLLER_AURA_H_
#define ASH_AURA_WM_ROOT_CONTROLLER_AURA_H_

#include "ash/ash_export.h"
#include "ash/common/wm_root_window_controller.h"
#include "base/macros.h"

namespace aura {
class Window;
}

namespace ash {

class RootWindowController;

class ASH_EXPORT WmRootWindowControllerAura : public WmRootWindowController {
 public:
  explicit WmRootWindowControllerAura(
      RootWindowController* root_window_controller);
  ~WmRootWindowControllerAura() override;

  static WmRootWindowControllerAura* Get(aura::Window* window) {
    return const_cast<WmRootWindowControllerAura*>(
        Get(const_cast<const aura::Window*>(window)));
  }
  static const WmRootWindowControllerAura* Get(const aura::Window* window);

  // WmRootWindowController:
  bool HasShelf() override;
  WmShell* GetShell() override;
  WmShelf* GetShelf() override;
  WmWindow* GetWindow() override;
  void ConfigureWidgetInitParamsForContainer(
      views::Widget* widget,
      int shell_container_id,
      views::Widget::InitParams* init_params) override;
  WmWindow* FindEventTarget(const gfx::Point& location_in_screen) override;
  gfx::Point GetLastMouseLocationInRoot() override;
  void OnInitialWallpaperAnimationStarted() override;
  void OnWallpaperAnimationFinished(views::Widget* widget) override;

 protected:
  // WmRootWindowController:
  bool ShouldDestroyWindowInCloseChildWindows(WmWindow* window) override;

 private:
  friend class RootWindowController;

  RootWindowController* root_window_controller_;

  DISALLOW_COPY_AND_ASSIGN(WmRootWindowControllerAura);
};

}  // namespace ash

#endif  // ASH_AURA_WM_ROOT_CONTROLLER_AURA_H_
