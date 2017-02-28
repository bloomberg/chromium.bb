// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SHELF_OVERFLOW_BUTTON_H_
#define ASH_COMMON_SHELF_OVERFLOW_BUTTON_H_

#include "base/macros.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/controls/button/custom_button.h"

namespace ash {
class ShelfView;
class WmShelf;

// Shelf overflow chevron button.
class OverflowButton : public views::CustomButton {
 public:
  // |shelf_view| is the view containing this button.
  OverflowButton(ShelfView* shelf_view, WmShelf* wm_shelf);
  ~OverflowButton() override;

  void OnShelfAlignmentChanged();
  void OnOverflowBubbleShown();
  void OnOverflowBubbleHidden();

  // Updates background and schedules a paint.
  void UpdateShelfItemBackground(SkColor color);

 private:
  // views::CustomButton:
  void OnPaint(gfx::Canvas* canvas) override;
  std::unique_ptr<views::InkDrop> CreateInkDrop() override;
  std::unique_ptr<views::InkDropRipple> CreateInkDropRipple() const override;
  bool ShouldEnterPushedState(const ui::Event& event) override;
  void NotifyClick(const ui::Event& event) override;
  std::unique_ptr<views::InkDropMask> CreateInkDropMask() const override;

  // Helper functions to paint the background and foreground of the button
  // at |bounds|.
  void PaintBackground(gfx::Canvas* canvas, const gfx::Rect& bounds);
  void PaintForeground(gfx::Canvas* canvas, const gfx::Rect& bounds);

  // Calculates the bounds of the overflow button based on the shelf alignment.
  gfx::Rect CalculateButtonBounds() const;

  // Used for bottom shelf alignment.
  gfx::ImageSkia bottom_image_;

  // Cached rotations of |bottom_image_| used for left and right shelf
  // alignments.
  gfx::ImageSkia left_image_;
  gfx::ImageSkia right_image_;

  ShelfView* shelf_view_;
  WmShelf* wm_shelf_;

  // Color used to paint the background.
  SkColor background_color_;

  DISALLOW_COPY_AND_ASSIGN(OverflowButton);
};

}  // namespace ash

#endif  // ASH_COMMON_SHELF_OVERFLOW_BUTTON_H_
