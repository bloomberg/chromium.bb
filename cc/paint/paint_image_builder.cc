// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/paint_image_builder.h"

namespace cc {

PaintImageBuilder::PaintImageBuilder() = default;
PaintImageBuilder::PaintImageBuilder(PaintImage image)
    : paint_image_(std::move(image)) {
#if DCHECK_IS_ON()
  id_set_ = true;
#endif
  paint_image_.cached_sk_image_ = nullptr;
  paint_image_.sk_image_ = nullptr;
  paint_image_.paint_record_ = nullptr;
  paint_image_.paint_record_rect_ = gfx::Rect();
  paint_image_.paint_image_generator_ = nullptr;
}
PaintImageBuilder::~PaintImageBuilder() = default;

PaintImage PaintImageBuilder::TakePaintImage() const {
#if DCHECK_IS_ON()
  DCHECK(id_set_);
  if (paint_image_.sk_image_) {
    DCHECK(!paint_image_.paint_record_);
    DCHECK(!paint_image_.paint_image_generator_);
    DCHECK(!paint_image_.sk_image_->isLazyGenerated());
    // TODO(khushalsagar): Assert that we don't have an animated image type
    // here.
  } else if (paint_image_.paint_record_) {
    DCHECK(!paint_image_.sk_image_);
    DCHECK(!paint_image_.paint_image_generator_);
    // TODO(khushalsagar): Assert that we don't have an animated image type
    // here.
  } else if (paint_image_.paint_image_generator_) {
    DCHECK(!paint_image_.sk_image_);
    DCHECK(!paint_image_.paint_record_);
  }
#endif

  return std::move(paint_image_);
}

}  // namespace cc
