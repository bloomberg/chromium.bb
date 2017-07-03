// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/thumbnails/simple_thumbnail_crop.h"

#include "content/public/browser/browser_thread.h"
#include "ui/gfx/geometry/size_conversions.h"

namespace thumbnails {

SimpleThumbnailCrop::SimpleThumbnailCrop(const gfx::Size& target_size)
    : target_size_(target_size) {
  DCHECK(!target_size.IsEmpty());
}

ClipResult SimpleThumbnailCrop::GetCanvasCopyInfo(const gfx::Size& source_size,
                                                  ui::ScaleFactor scale_factor,
                                                  gfx::Rect* clipping_rect,
                                                  gfx::Size* copy_size) const {
  DCHECK(!source_size.IsEmpty());
  ClipResult clip_result = thumbnails::CLIP_RESULT_NOT_CLIPPED;
  *clipping_rect = GetClippingRect(source_size, target_size_, &clip_result);
  *copy_size = GetCopySizeForThumbnail(scale_factor, target_size_);
  return clip_result;
}

// RenderWidgetHostView::CopyFromSurface() can be costly especially when it is
// necessary to read back the web contents image data from GPU. As the cost is
// roughly proportional to the number of the copied pixels, the size of the
// copied pixels should be as small as possible.
// static
gfx::Size SimpleThumbnailCrop::GetCopySizeForThumbnail(
    ui::ScaleFactor scale_factor,
    const gfx::Size& thumbnail_size) {
  // The copy size returned is the pixel equivalent of |thumbnail_size|, which
  // is in DIPs.
  if (scale_factor == ui::SCALE_FACTOR_100P) {
    // In the case of 1x devices, we get a thumbnail twice as big and reduce
    // it at serve time to improve quality.
    scale_factor = ui::SCALE_FACTOR_200P;
  }
  float scale = GetScaleForScaleFactor(scale_factor);
  return gfx::ScaleToFlooredSize(thumbnail_size, scale);
}

gfx::Rect SimpleThumbnailCrop::GetClippingRect(const gfx::Size& source_size,
                                               const gfx::Size& desired_size,
                                               ClipResult* clip_result) {
  DCHECK(clip_result);

  float desired_aspect =
      static_cast<float>(desired_size.width()) / desired_size.height();

  // Get the clipping rect so that we can preserve the aspect ratio while
  // filling the destination.
  gfx::Rect clipping_rect;
  if (source_size.width() < desired_size.width() ||
      source_size.height() < desired_size.height()) {
    // Source image is smaller: we clip the part of source image within the
    // dest rect, and then stretch it to fill the dest rect. We don't respect
    // the aspect ratio in this case.
    clipping_rect = gfx::Rect(desired_size);
    *clip_result = thumbnails::CLIP_RESULT_SOURCE_IS_SMALLER;
  } else {
    float src_aspect =
        static_cast<float>(source_size.width()) / source_size.height();
    if (src_aspect > desired_aspect) {
      // Wider than tall, clip horizontally: we center the smaller
      // thumbnail in the wider screen.
      int new_width = static_cast<int>(source_size.height() * desired_aspect);
      int x_offset = (source_size.width() - new_width) / 2;
      clipping_rect.SetRect(x_offset, 0, new_width, source_size.height());
      *clip_result = (src_aspect >= ThumbnailScore::kTooWideAspectRatio) ?
          thumbnails::CLIP_RESULT_MUCH_WIDER_THAN_TALL :
          thumbnails::CLIP_RESULT_WIDER_THAN_TALL;
    } else if (src_aspect < desired_aspect) {
      clipping_rect =
          gfx::Rect(source_size.width(), source_size.width() / desired_aspect);
      *clip_result = thumbnails::CLIP_RESULT_TALLER_THAN_WIDE;
    } else {
      clipping_rect = gfx::Rect(source_size);
      *clip_result = thumbnails::CLIP_RESULT_NOT_CLIPPED;
    }
  }
  return clipping_rect;
}

SimpleThumbnailCrop::~SimpleThumbnailCrop() = default;

} // namespace thumbnails
