// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DRAG_DROP_DRAG_IMAGE_VIEW_H_
#define ASH_DRAG_DROP_DRAG_IMAGE_VIEW_H_

#include "ui/views/controls/image_view.h"

namespace views {
class Widget;
}

namespace ash {
namespace internal {

class DragImageView : public views::ImageView {
 public:
  explicit DragImageView(gfx::NativeView context);
  virtual ~DragImageView();

  // Sets the bounds of the native widget in screen
  // coordinates.
  // TODO(oshima): Looks like this is root window's
  // coordinate. Change this to screen's coordinate.
  void SetBoundsInScreen(const gfx::Rect& bounds);

  // Sets the position of the native widget.
  void SetScreenPosition(const gfx::Point& position);

  // Sets the visibility of the native widget.
  void SetWidgetVisible(bool visible);

 private:
  // Overridden from views::ImageView.
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;

  scoped_ptr<views::Widget> widget_;
  gfx::Size widget_size_;

  DISALLOW_COPY_AND_ASSIGN(DragImageView);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_DRAG_DROP_DRAG_IMAGE_VIEW_H_
