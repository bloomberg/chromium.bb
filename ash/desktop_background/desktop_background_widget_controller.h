// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DESKTOP_BACKGROUND_DESKTOP_BACKGROUND_WIDGET_CONTROLLER_H_
#define ASH_DESKTOP_BACKGROUND_DESKTOP_BACKGROUND_WIDGET_CONTROLLER_H_

#include "base/memory/scoped_ptr.h"
#include "ui/aura/window_property.h"
#include "ui/compositor/layer.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace internal {

// This class hides difference between two possible background implementations:
// effective Layer-based for solid color, and Widget-based for images.
// DesktopBackgroundWidgetController is installed as an owned property on the
// RootWindow. To avoid a white flash during wallpaper changes the old
// DesktopBackgroundWidgetController is moved to a secondary property
// (kComponentWrapper). When the animation completes the old
// DesktopBackgroundWidgetController is destroyed.
class DesktopBackgroundWidgetController {
 public:
  // Create
  explicit DesktopBackgroundWidgetController(views::Widget* widget);
  explicit DesktopBackgroundWidgetController(ui::Layer* layer);

  ~DesktopBackgroundWidgetController();

  // Drop widget reference. widget is not owned.
  void CleanupWidget();

  // Set bounds of component that draws background.
  void SetBounds(gfx::Rect bounds);

  // Move component from |src_container| in |root_window| to |dest_container|.
  // It is required for lock screen, when we need to move background so that
  // it hides user's windows.
  void Reparent(aura::RootWindow* root_window,
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
// the kWindowDesktopComponent property on RootWindow is set to the
// DesktopBackgroundWidgetController in this class.
class ComponentWrapper {
 public:
  explicit ComponentWrapper(
      DesktopBackgroundWidgetController* component);
  ~ComponentWrapper();

  // Gets the wrapped DesktopBackgroundWidgetController pointer. Caller should
  // take ownership of the pointer if |pass_ownership| is true.
  DesktopBackgroundWidgetController* GetComponent(bool pass_ownership);

 private:
  scoped_ptr<DesktopBackgroundWidgetController> component_;

  DISALLOW_COPY_AND_ASSIGN(ComponentWrapper);
};

// Window property key, that binds instance of DesktopBackgroundWidgetController
// to root windows.
extern const aura::WindowProperty<DesktopBackgroundWidgetController*>* const
    kWindowDesktopComponent;

extern const aura::WindowProperty<ComponentWrapper*>* const kComponentWrapper;

}  // namespace internal
}  // namespace ash

#endif  // ASH_DESKTOP_BACKGROUND_DESKTOP_BACKGROUND_WIDGET_CONTROLLER_H_
