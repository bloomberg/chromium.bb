// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_FIXED_SIZED_IMAGE_VIEW_H_
#define ASH_SYSTEM_TRAY_FIXED_SIZED_IMAGE_VIEW_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/views/controls/image_view.h"

namespace ash {

// An image view with a specified width and height (kTrayPopupDetailsIconWidth).
// If the specified width or height is zero, then the image size is used for
// that dimension.
class FixedSizedImageView : public views::ImageView {
 public:
  FixedSizedImageView(int width, int height);
  virtual ~FixedSizedImageView();

 private:
  virtual gfx::Size GetPreferredSize() const OVERRIDE;

  int width_;
  int height_;

  DISALLOW_COPY_AND_ASSIGN(FixedSizedImageView);
};

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_FIXED_SIZED_IMAGE_VIEW_H_
