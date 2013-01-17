// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/picture_pile_base.h"

#include "base/logging.h"

namespace cc {

PicturePileBase::PicturePileBase()
    : min_contents_scale_(0),
      buffer_pixels_(0) {
  SetMinContentsScale(1);
}

PicturePileBase::~PicturePileBase() {
}

void PicturePileBase::Resize(gfx::Size size) {
  if (size_ == size)
    return;

  pile_.clear();
  size_ = size;
}

void PicturePileBase::SetMinContentsScale(float min_contents_scale) {
  DCHECK(min_contents_scale);
  if (min_contents_scale_ == min_contents_scale)
    return;

  pile_.clear();
  min_contents_scale_ = min_contents_scale;

  // Picture contents are played back scaled. When the final contents scale is
  // less than 1 (i.e. low res), then multiple recorded pixels will be used
  // to raster one final pixel.  To avoid splitting a final pixel across
  // pictures (which would result in incorrect rasterization due to blending), a
  // buffer margin is added so that any picture can be snapped to integral
  // final pixels.
  //
  // For example, if a 1/4 contents scale is used, then that would be 3 buffer
  // pixels, since that's the minimum number of pixels to add so that resulting
  // content can be snapped to a four pixel aligned grid.
  buffer_pixels_ = static_cast<int>(ceil(1 / min_contents_scale_) - 1);
  buffer_pixels_ = std::max(0, buffer_pixels_);
}

void PicturePileBase::PushPropertiesTo(PicturePileBase* other) {
  other->pile_ = pile_;
  other->size_ = size_;
  other->min_contents_scale_ = min_contents_scale_;
  other->buffer_pixels_ = buffer_pixels_;
}

}  // namespace cc
