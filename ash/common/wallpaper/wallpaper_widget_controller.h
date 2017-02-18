// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_WALLPAPER_WALLPAPER_WIDGET_CONTROLLER_H_
#define ASH_COMMON_WALLPAPER_WALLPAPER_WIDGET_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "base/macros.h"
#include "ui/aura/window_observer.h"
#include "ui/views/widget/widget_observer.h"

namespace ash {

class RootWindowController;
class WmWindow;

// This class implements a widget-based wallpaper.
// WallpaperWidgetController is owned by RootWindowController.
// When the animation completes the old WallpaperWidgetController is
// destroyed. Exported for tests.
class ASH_EXPORT WallpaperWidgetController : public views::WidgetObserver,
                                             public aura::WindowObserver {
 public:
  explicit WallpaperWidgetController(views::Widget* widget);
  ~WallpaperWidgetController() override;

  // Overridden from views::WidgetObserver.
  void OnWidgetDestroying(views::Widget* widget) override;

  // Set bounds for the widget that draws the wallpaper.
  void SetBounds(const gfx::Rect& bounds);

  // Move the wallpaper for |root_window| to the specified |container|.
  // The lock screen moves the wallpaper container to hides the user's windows.
  // Returns true if there was something to reparent.
  bool Reparent(WmWindow* root_window, int container);

  // Starts wallpaper fade in animation. |root_window_controller| is
  // the root window where the animation will happen. (This is
  // necessary this as |layer_| doesn't have access to the root window).
  void StartAnimating(RootWindowController* root_window_controller);

  views::Widget* widget() { return widget_; }

 private:
  void RemoveObservers();

  // aura::WindowObserver:
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds) override;

  views::Widget* widget_;

  // Parent of |widget_|.
  WmWindow* widget_parent_;

  DISALLOW_COPY_AND_ASSIGN(WallpaperWidgetController);
};

// This class wraps a WallpaperWidgetController pointer. It is owned
// by RootWindowController. The instance of WallpaperWidgetController is
// moved to this RootWindowController when the animation completes.
// Exported for tests.
class ASH_EXPORT AnimatingWallpaperWidgetController {
 public:
  explicit AnimatingWallpaperWidgetController(
      WallpaperWidgetController* component);
  ~AnimatingWallpaperWidgetController();

  // Stops animation and makes sure OnImplicitAnimationsCompleted() is called if
  // current animation is not finished yet.
  // Note we have to make sure this function is called before we set
  // AnimatingWallpaperWidgetController to a new controller. If it is not
  // called, the animating widget/layer is closed immediately and the new one is
  // animating from the widget/layer before animation. For instance, if a user
  // quickly switches between red, green and blue wallpapers. The green
  // wallpaper will first fade in from red wallpaper. And in the middle of the
  // animation, blue wallpaper also wants to fade in. If the green wallpaper
  // animation does not finish immediately, the green wallpaper widget will be
  // removed and the red widget will show again. As a result, the blue wallpaper
  // fades in from red wallpaper. This is a bad user experience. See bug
  // http://crbug.com/156542 for more details.
  void StopAnimating();

  // Gets the wrapped WallpaperWidgetController pointer. Caller should
  // take ownership of the pointer if |pass_ownership| is true.
  WallpaperWidgetController* GetController(bool pass_ownership);

 private:
  std::unique_ptr<WallpaperWidgetController> controller_;

  DISALLOW_COPY_AND_ASSIGN(AnimatingWallpaperWidgetController);
};

}  // namespace ash

#endif  // ASH_COMMON_WALLPAPER_WALLPAPER_WIDGET_CONTROLLER_H_
