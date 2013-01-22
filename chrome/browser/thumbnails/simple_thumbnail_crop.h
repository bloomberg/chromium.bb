// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_THUMBNAILS_SIMPLE_THUMBNAIL_CROP_H_
#define CHROME_BROWSER_THUMBNAILS_SIMPLE_THUMBNAIL_CROP_H_

#include "chrome/browser/thumbnails/thumbnailing_algorithm.h"

namespace thumbnails {

// The implementation of the 'classic' thumbnail cropping algorithm. It is not
// content-driven in any meaningful way (save for score calculation). Rather,
// the choice of a cropping region is based on relation between source and
// target sizes. The selected source region is then rescaled into the target
// thumbnail image.
class SimpleThumbnailCrop : public ThumbnailingAlgorithm {
 public:
  explicit SimpleThumbnailCrop(const gfx::Size& target_size);

  virtual ClipResult GetCanvasCopyInfo(const gfx::Size& source_size,
                                       ui::ScaleFactor scale_factor,
                                       gfx::Rect* clipping_rect,
                                       gfx::Size* target_size) const OVERRIDE;

  virtual void ProcessBitmap(ThumbnailingContext* context,
                             const ConsumerCallback& callback,
                             const SkBitmap& bitmap) OVERRIDE;

  // Calculates how "boring" a thumbnail is. The boring score is the
  // 0,1 ranged percentage of pixels that are the most common
  // luma. Higher boring scores indicate that a higher percentage of a
  // bitmap are all the same brightness.
  // Statically exposed for use by tests only.
  static double CalculateBoringScore(const SkBitmap& bitmap);

  // Gets the clipped bitmap from |bitmap| per the aspect ratio of the
  // desired width and the desired height. For instance, if the input
  // bitmap is vertically long (ex. 400x900) and the desired size is
  // square (ex. 100x100), the clipped bitmap will be the top half of the
  // input bitmap (400x400).
  // Statically exposed for use by tests only.
  static SkBitmap GetClippedBitmap(const SkBitmap& bitmap,
                                   int desired_width,
                                   int desired_height,
                                   thumbnails::ClipResult* clip_result);
  static gfx::Size GetCopySizeForThumbnail(ui::ScaleFactor scale_factor,
                                           const gfx::Size& thumbnail_size);

 protected:
  virtual ~SimpleThumbnailCrop();

 private:
  gfx::Size GetThumbnailSizeInPixel() const;
  static gfx::Rect GetClippingRect(const gfx::Size& source_size,
                                   const gfx::Size& desired_size,
                                   ClipResult* clip_result);
  static SkBitmap CreateThumbnail(const SkBitmap& bitmap,
                                  const gfx::Size& desired_size,
                                  ClipResult* clip_result);

  const gfx::Size target_size_;

  DISALLOW_COPY_AND_ASSIGN(SimpleThumbnailCrop);
};

}

#endif  // CHROME_BROWSER_THUMBNAILS_SIMPLE_THUMBNAIL_CROP_H_
