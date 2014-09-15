// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_APP_LIST_BUTTON_H_
#define ASH_SHELF_APP_LIST_BUTTON_H_

#include "ui/views/controls/button/image_button.h"

namespace ash {
class ShelfButtonHost;
class ShelfWidget;

// Button used for the AppList icon on the shelf.
class AppListButton : public views::ImageButton {
 public:
  // Bounds size (inset) required for the app icon image (in pixels).
  static const int kImageBoundsSize;

  AppListButton(views::ButtonListener* listener,
                ShelfButtonHost* host,
                ShelfWidget* shelf_widget);
  virtual ~AppListButton();

  bool draw_background_as_active() {
    return draw_background_as_active_;
  }

 protected:
  // views::ImageButton overrides:
  virtual bool OnMousePressed(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnMouseReleased(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnMouseCaptureLost() OVERRIDE;
  virtual bool OnMouseDragged(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnMouseMoved(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnMouseEntered(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnMouseExited(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;
  virtual void GetAccessibleState(ui::AXViewState* state) OVERRIDE;

  // ui::EventHandler overrides:
  virtual void OnGestureEvent(ui::GestureEvent* event) OVERRIDE;

 private:
  // Toggles the active state for painting the background and schedules a paint.
  void SetDrawBackgroundAsActive(bool draw_background_as_active);

  // True if the background should render as active, regardless of the state of
  // the application list.
  bool draw_background_as_active_;

  // True if touch view feedback command line flag has been enabled. When
  // enabled touch gestures will toggle rendering the background as active.
  bool touch_feedback_enabled_;

  ShelfButtonHost* host_;
  // Reference to the shelf widget containing this button, owned by the
  // root window controller.
  ShelfWidget* shelf_widget_;

  DISALLOW_COPY_AND_ASSIGN(AppListButton);
};

}  // namespace ash

#endif  // ASH_SHELF_APP_LIST_BUTTON_H_
