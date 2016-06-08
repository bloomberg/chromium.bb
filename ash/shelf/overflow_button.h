// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_OVERFLOW_BUTTON_H_
#define ASH_SHELF_OVERFLOW_BUTTON_H_

#include "ash/common/shelf/shelf_types.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/controls/button/custom_button.h"

namespace ash {

class Shelf;

// Shelf overflow chevron button.
class OverflowButton : public views::CustomButton {
 public:
  OverflowButton(views::ButtonListener* listener, Shelf* shelf);
  ~OverflowButton() override;

  void OnShelfAlignmentChanged();

 private:
  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;

  // Helper functions to paint the background and foreground of the button
  // at |bounds|.
  void PaintBackground(gfx::Canvas* canvas, const gfx::Rect& bounds);
  void PaintForeground(gfx::Canvas* canvas, const gfx::Rect& bounds);

  // Returns the id of the asset to use for the button backgound based on the
  // current shelf state.
  // TODO(tdanderson): Remove this once the material design shelf is enabled
  // by default. See crbug.com/614453.
  int NonMaterialBackgroundImageId();

  // Calculates the bounds of the overflow button based on the shelf alignment.
  // TODO(tdanderson): The button size and bounds need to be adjusted to
  // conform to the material design spec. See crbug.com/617295.
  gfx::Rect CalculateButtonBounds();

  // Left and right images are rotations of bottom_image and are
  // owned by the overflow button.
  gfx::ImageSkia left_image_;
  gfx::ImageSkia right_image_;
  // Bottom image is owned by the resource bundle.
  const gfx::ImageSkia* bottom_image_;
  Shelf* shelf_;

  DISALLOW_COPY_AND_ASSIGN(OverflowButton);
};

}  // namespace ash

#endif  // ASH_SHELF_OVERFLOW_BUTTON_H_
