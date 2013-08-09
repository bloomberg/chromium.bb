// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LAUNCHER_OVERFLOW_BUTTON_H_
#define ASH_LAUNCHER_OVERFLOW_BUTTON_H_

#include "ash/shelf/shelf_types.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/views/controls/button/custom_button.h"

namespace gfx {
class ImageSkia;
}

namespace ash {
namespace internal {

// Launcher overflow chevron button.
class OverflowButton : public views::CustomButton {
 public:
  explicit OverflowButton(views::ButtonListener* listener);
  virtual ~OverflowButton();

  void OnShelfAlignmentChanged();

 private:
  void PaintBackground(gfx::Canvas* canvas, int alpha);

  // views::View overrides:
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;

  const gfx::ImageSkia* image_;

  DISALLOW_COPY_AND_ASSIGN(OverflowButton);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_LAUNCHER_OVERFLOW_BUTTON_H_
