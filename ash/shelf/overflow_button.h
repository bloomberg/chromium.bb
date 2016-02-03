// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_OVERFLOW_BUTTON_H_
#define ASH_SHELF_OVERFLOW_BUTTON_H_

#include "ash/shelf/shelf_types.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/controls/button/custom_button.h"

namespace ash {

class ShelfLayoutManager;

// Shelf overflow chevron button.
class OverflowButton : public views::CustomButton {
 public:
  OverflowButton(views::ButtonListener* listener, ShelfLayoutManager* shelf);
  ~OverflowButton() override;

  void OnShelfAlignmentChanged();

 private:
  void PaintBackground(gfx::Canvas* canvas, int alpha);

  // views::View overrides:
  void OnPaint(gfx::Canvas* canvas) override;

  // Left and right images are rotations of bottom_image and are
  // owned by the overflow button.
  gfx::ImageSkia left_image_;
  gfx::ImageSkia right_image_;
  // Bottom image is owned by the resource bundle.
  const gfx::ImageSkia* bottom_image_;
  ShelfLayoutManager* shelf_;

  DISALLOW_COPY_AND_ASSIGN(OverflowButton);
};

}  // namespace ash

#endif  // ASH_SHELF_OVERFLOW_BUTTON_H_
