// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_TRAY_POPUP_ITEM_CONTAINER_H_
#define ASH_SYSTEM_TRAY_TRAY_POPUP_ITEM_CONTAINER_H_

#include "ui/views/view.h"

namespace ash {

// A view which can optionally change the background color when a mouse is
// hovering or a user is interacting via touch.
class TrayPopupItemContainer : public views::View {
 public:
  TrayPopupItemContainer(views::View* view,
                         bool change_background,
                         bool draw_border);

  ~TrayPopupItemContainer() override;

  bool active() {
    return active_;
  }

 private:
  // Sets whether the active background is to be used, and triggers a paint.
  void SetActive(bool active);

  // views::View:
  void ChildVisibilityChanged(views::View* child) override;
  void ChildPreferredSizeChanged(views::View* child) override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  void OnPaintBackground(gfx::Canvas* canvas) override;

  // True if either a mouse is hovering over this view, or if a user has touched
  // down.
  bool active_;

  // True if mouse hover and touch feedback can alter the background color of
  // the container.
  bool change_background_;

  DISALLOW_COPY_AND_ASSIGN(TrayPopupItemContainer);
};

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_TRAY_POPUP_ITEM_CONTAINER_H_
