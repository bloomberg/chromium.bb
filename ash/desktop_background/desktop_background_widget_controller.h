// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DESKTOP_BACKGROUND_DESKTOP_BACKGROUND_WIDGET_CONTROLLER_H_
#define ASH_DESKTOP_BACKGROUND_DESKTOP_BACKGROUND_WIDGET_CONTROLLER_H_

#include "ash/ash_export.h"
#include "base/memory/scoped_ptr.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace ash {
class RootWindowController;

// This class implements a widget-based wallpaper.
// DesktopBackgroundWidgetController is owned by RootWindowController.
// When the animation completes the old DesktopBackgroundWidgetController is
// destroyed. Exported for tests.
class ASH_EXPORT DesktopBackgroundWidgetController
    : public views::WidgetObserver {
 public:
  // Create
  explicit DesktopBackgroundWidgetController(views::Widget* widget);

  virtual ~DesktopBackgroundWidgetController();

  // Overridden from views::WidgetObserver.
  virtual void OnWidgetDestroying(views::Widget* widget) OVERRIDE;

  // Set bounds of component that draws background.
  void SetBounds(gfx::Rect bounds);

  // Move component from |src_container| in |root_window| to |dest_container|.
  // It is required for lock screen, when we need to move background so that
  // it hides user's windows. Returns true if there was something to reparent.
  bool Reparent(aura::Window* root_window,
                int src_container,
                int dest_container);

  // Starts wallpaper fade in animation. |root_window_controller| is
  // the root window where the animation will happen. (This is
  // necessary this as |layer_| doesn't have access to the root window).
  void StartAnimating(RootWindowController* root_window_controller);

  views::Widget* widget() { return widget_; }

 private:
  views::Widget* widget_;

  DISALLOW_COPY_AND_ASSIGN(DesktopBackgroundWidgetController);
};

// This class wraps a DesktopBackgroundWidgetController pointer. It is owned
// by RootWindowController. The instance of DesktopBackgroundWidgetController is
// moved to this RootWindowController when the animation completes.
// Exported for tests.
class ASH_EXPORT AnimatingDesktopController {
 public:
  explicit AnimatingDesktopController(
      DesktopBackgroundWidgetController* component);
  ~AnimatingDesktopController();

  // Stops animation and makes sure OnImplicitAnimationsCompleted() is called if
  // current animation is not finished yet.
  // Note we have to make sure this function is called before we set
  // kAnimatingDesktopController to a new controller. If it is not called, the
  // animating widget/layer is closed immediately and the new one is animating
  // from the widget/layer before animation. For instance, if a user quickly
  // switches between red, green and blue wallpapers. The green wallpaper will
  // first fade in from red wallpaper. And in the middle of the animation, blue
  // wallpaper also wants to fade in. If the green wallpaper animation does not
  // finish immediately, the green wallpaper widget will be removed and the red
  // widget will show again. As a result, the blue wallpaper fades in from red
  // wallpaper. This is a bad user experience. See bug http://crbug.com/156542
  // for more details.
  void StopAnimating();

  // Gets the wrapped DesktopBackgroundWidgetController pointer. Caller should
  // take ownership of the pointer if |pass_ownership| is true.
  DesktopBackgroundWidgetController* GetController(bool pass_ownership);

 private:
  scoped_ptr<DesktopBackgroundWidgetController> controller_;

  DISALLOW_COPY_AND_ASSIGN(AnimatingDesktopController);
};

}  // namespace ash

#endif  // ASH_DESKTOP_BACKGROUND_DESKTOP_BACKGROUND_WIDGET_CONTROLLER_H_
