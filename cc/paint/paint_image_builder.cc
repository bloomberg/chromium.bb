// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/paint_image_builder.h"

namespace cc {

PaintImageBuilder::PaintImageBuilder() = default;
PaintImageBuilder::~PaintImageBuilder() = default;

PaintImage PaintImageBuilder::TakePaintImage() const {
  DCHECK(!paint_image_.sk_image_ || !paint_image_.paint_record_);
  return std::move(paint_image_);
}

}  // namespace cc
