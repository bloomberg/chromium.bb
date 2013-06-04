// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_THUMBNAILS_CONTENT_BASED_THUMBNAILING_ALGORITHM_H_
#define CHROME_BROWSER_THUMBNAILS_CONTENT_BASED_THUMBNAILING_ALGORITHM_H_

#include "chrome/browser/thumbnails/thumbnailing_algorithm.h"

namespace thumbnails {

// Encapsulates a method of creating a thumbnail from a captured tab shot which
// attempts to preserve only relevant fragments of the original image.
// The algorithm detects areas of high activity at low resolution and discards
// rows and columns which do not intersect with these areas.
class ContentBasedThumbnailingAlgorithm : public ThumbnailingAlgorithm {
 public:
  explicit ContentBasedThumbnailingAlgorithm(const gfx::Size& target_size);

  virtual ClipResult GetCanvasCopyInfo(const gfx::Size& source_size,
                                       ui::ScaleFactor scale_factor,
                                       gfx::Rect* clipping_rect,
                                       gfx::Size* target_size) const OVERRIDE;

  virtual void ProcessBitmap(scoped_refptr<ThumbnailingContext> context,
                             const ConsumerCallback& callback,
                             const SkBitmap& bitmap) OVERRIDE;

  // Prepares (clips to size, copies etc.) the bitmap passed to ProcessBitmap.
  // Always returns a bitmap that can be properly refcounted.
  // Extracted and exposed as a test seam.
  static SkBitmap PrepareSourceBitmap(const SkBitmap& received_bitmap,
                                      const gfx::Size& thumbnail_size,
                                      ThumbnailingContext* context);

  // The function processes |source_bitmap| into a thumbnail of |thumbnail_size|
  // and passes the result into |callback| (on UI thread). |context| describes
  // how the thumbnail is being created.
  static void CreateRetargetedThumbnail(
      const SkBitmap& source_bitmap,
      const gfx::Size& thumbnail_size,
      scoped_refptr<ThumbnailingContext> context,
      const ConsumerCallback& callback);

 protected:
  virtual ~ContentBasedThumbnailingAlgorithm();

 private:
  static gfx::Rect GetClippingRect(const gfx::Size& source_size,
                                   const gfx::Size& thumbnail_size,
                                   gfx::Size* target_size,
                                   ClipResult* clip_result);

  const gfx::Size target_size_;

  DISALLOW_COPY_AND_ASSIGN(ContentBasedThumbnailingAlgorithm);
};

}  // namespace thumbnails

#endif  // CHROME_BROWSER_THUMBNAILS_CONTENT_BASED_THUMBNAILING_ALGORITHM_H_
