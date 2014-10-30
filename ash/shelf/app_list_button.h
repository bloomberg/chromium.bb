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
  ~AppListButton() override;

  bool draw_background_as_active() {
    return draw_background_as_active_;
  }

 protected:
  // views::ImageButton overrides:
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnMouseCaptureLost() override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  void OnMouseMoved(const ui::MouseEvent& event) override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  void OnPaint(gfx::Canvas* canvas) override;
  void GetAccessibleState(ui::AXViewState* state) override;

  // ui::EventHandler overrides:
  void OnGestureEvent(ui::GestureEvent* event) override;

 private:
  // Toggles the active state for painting the background and schedules a paint.
  void SetDrawBackgroundAsActive(bool draw_background_as_active);

  // True if the background should render as active, regardless of the state of
  // the application list.
  bool draw_background_as_active_;

  ShelfButtonHost* host_;
  // Reference to the shelf widget containing this button, owned by the
  // root window controller.
  ShelfWidget* shelf_widget_;

  DISALLOW_COPY_AND_ASSIGN(AppListButton);
};

}  // namespace ash

#endif  // ASH_SHELF_APP_LIST_BUTTON_H_
