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

  ClipResult GetCanvasCopyInfo(const gfx::Size& source_size,
                               ui::ScaleFactor scale_factor,
                               gfx::Rect* clipping_rect,
                               gfx::Size* copy_size) const override;

  void ProcessBitmap(scoped_refptr<ThumbnailingContext> context,
                     const ConsumerCallback& callback,
                     const SkBitmap& bitmap) override;

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
  // Returns the size copied from the backing store. |thumbnail_size| is in
  // DIP, returned size in pixels.
  static gfx::Size GetCopySizeForThumbnail(ui::ScaleFactor scale_factor,
                                           const gfx::Size& thumbnail_size);
  static gfx::Rect GetClippingRect(const gfx::Size& source_size,
                                   const gfx::Size& desired_size,
                                   ClipResult* clip_result);

  // Computes the size of a thumbnail that should be stored in the database from
  // |given_size| (expected to be the thumbnail size we would normally want to
  // see). The returned size is expressed in pixels and is determined by
  // bumping the resolution up to the maximum scale factor.
  static gfx::Size ComputeTargetSizeAtMaximumScale(const gfx::Size& given_size);

 protected:
  ~SimpleThumbnailCrop() override;

 private:
  static SkBitmap CreateThumbnail(const SkBitmap& bitmap,
                                  const gfx::Size& desired_size,
                                  ClipResult* clip_result);

  // The target size of the captured thumbnails, in DIPs.
  const gfx::Size target_size_;

  DISALLOW_COPY_AND_ASSIGN(SimpleThumbnailCrop);
};

}  // namespace thumbnails

#endif  // CHROME_BROWSER_THUMBNAILS_SIMPLE_THUMBNAIL_CROP_H_
