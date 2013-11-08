// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MAGNIFIER_PARTIAL_MAGNIFICATION_CONTROLLER_H_
#define ASH_MAGNIFIER_PARTIAL_MAGNIFICATION_CONTROLLER_H_

#include "ui/aura/window_observer.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/point.h"
#include "ui/views/widget/widget_observer.h"

namespace ash {

const float kDefaultPartialMagnifiedScale = 1.5f;
const float kNonPartialMagnifiedScale = 1.0f;

// Controls the partial screen magnifier, which is a small area of the screen
// which is zoomed in.  The zoomed area follows the mouse cursor when enabled.
class PartialMagnificationController
  : public ui::EventHandler,
    public aura::WindowObserver,
    public views::WidgetObserver {
 public:
  PartialMagnificationController();
  virtual ~PartialMagnificationController();

  // Enables (or disables if |enabled| is false) partial screen magnifier
  // feature.
  virtual void SetEnabled(bool enabled);

  bool is_enabled() const { return is_enabled_; }

  // Sets the magnification ratio. 1.0f means no magnification.
  void SetScale(float scale);

  // Returns the current magnification ratio.
  float GetScale() const { return scale_; }

 private:
  void OnMouseMove(const gfx::Point& location_in_root);

  // Switch PartialMagnified RootWindow to |new_root_window|. This does
  // following:
  //  - Remove the magnifier from the current root window.
  //  - Create a magnifier in the new root_window |new_root_window|.
  //  - Switch the target window from current window to |new_root_window|.
  void SwitchTargetRootWindow(aura::Window* new_root_window);

  // Returns the root window that contains the mouse cursor.
  aura::Window* GetCurrentRootWindow();

  // Return true if the magnification scale > kMinPartialMagnifiedScaleThreshold
  bool IsPartialMagnified() const;

  // Create the magnifier window.
  void CreateMagnifierWindow();

  // Cleans up the window if needed.
  void CloseMagnifierWindow();

  // Removes this as an observer of the zoom widget and the root window.
  void RemoveZoomWidgetObservers();

  // ui::EventHandler overrides:
  virtual void OnMouseEvent(ui::MouseEvent* event) OVERRIDE;

  // Overridden from WindowObserver:
  virtual void OnWindowDestroying(aura::Window* window) OVERRIDE;

  // Overridden from WidgetObserver:
  virtual void OnWidgetDestroying(views::Widget* widget) OVERRIDE;

  // True if the magnified window is in motion of zooming or un-zooming effect.
  // Otherwise, false.
  bool is_on_zooming_;

  bool is_enabled_;

  // Current scale, origin (left-top) position of the magnification window.
  float scale_;
  gfx::Point origin_;

  views::Widget* zoom_widget_;

  DISALLOW_COPY_AND_ASSIGN(PartialMagnificationController);
};

}  // namespace ash

#endif  // ASH_MAGNIFIER_PARTIAL_MAGNIFICATION_CONTROLLER_H_
