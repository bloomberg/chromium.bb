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
  ShelfButtonHost* host_;
  // Reference to the shelf widget containing this button, owned by the
  // root window controller.
  ShelfWidget* shelf_widget_;

  DISALLOW_COPY_AND_ASSIGN(AppListButton);
};

}  // namespace ash

#endif  // ASH_SHELF_APP_LIST_BUTTON_H_
