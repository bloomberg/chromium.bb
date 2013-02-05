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
namespace internal {

// This class hides difference between two possible background implementations:
// effective Layer-based for solid color, and Widget-based for images.
// DesktopBackgroundWidgetController is installed as an owned property on the
// RootWindow. To avoid a white flash during wallpaper changes the old
// DesktopBackgroundWidgetController is moved to a secondary property
// (kComponentWrapper). When the animation completes the old
// DesktopBackgroundWidgetController is destroyed. Exported for tests.
class ASH_EXPORT DesktopBackgroundWidgetController
    : public views::WidgetObserver {
 public:
  // Create
  explicit DesktopBackgroundWidgetController(views::Widget* widget);
  explicit DesktopBackgroundWidgetController(ui::Layer* layer);

  virtual ~DesktopBackgroundWidgetController();

  // Overridden from views::WidgetObserver.
  virtual void OnWidgetDestroying(views::Widget* widget) OVERRIDE;

  // Set bounds of component that draws background.
  void SetBounds(gfx::Rect bounds);

  // Move component from |src_container| in |root_window| to |dest_container|.
  // It is required for lock screen, when we need to move background so that
  // it hides user's windows. Returns true if there was something to reparent.
  bool Reparent(aura::RootWindow* root_window,
                int src_container,
                int dest_container);

  views::Widget* widget() { return widget_; }
  ui::Layer* layer() { return layer_.get(); }

 private:
  views::Widget* widget_;
  scoped_ptr<ui::Layer> layer_;

  DISALLOW_COPY_AND_ASSIGN(DesktopBackgroundWidgetController);
};

// This class wraps a DesktopBackgroundWidgetController pointer. It is installed
// as an owned property on the RootWindow. DesktopBackgroundWidgetController is
// moved to this property before animation completes. After animation completes,
// the kDesktopController property on RootWindow is set to the
// DesktopBackgroundWidgetController in this class. Exported for tests.
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

// Window property key, that binds instance of DesktopBackgroundWidgetController
// to root windows.  Owned property.
ASH_EXPORT extern
    const aura::WindowProperty<DesktopBackgroundWidgetController*>* const
        kDesktopController;

// Wrapper for the DesktopBackgroundWidgetController for a desktop background
// that is animating in.  Owned property.
ASH_EXPORT extern const aura::WindowProperty<AnimatingDesktopController*>* const
    kAnimatingDesktopController;

}  // namespace internal
}  // namespace ash

#endif  // ASH_DESKTOP_BACKGROUND_DESKTOP_BACKGROUND_WIDGET_CONTROLLER_H_
