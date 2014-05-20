// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_THUMBNAILS_THUMBNAILING_ALGORITHM_H_
#define CHROME_BROWSER_THUMBNAILS_THUMBNAILING_ALGORITHM_H_

#include "base/memory/ref_counted.h"
#include "chrome/browser/thumbnails/thumbnailing_context.h"
#include "ui/base/layout.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"

class SkBitmap;

namespace thumbnails {

// An interface abstracting thumbnailing algorithms. Instances are intended to
// be created by ThumbnailService's implementations and used by
// ThumbnailTabHelper as consumers of captured source images.
class ThumbnailingAlgorithm
    : public base::RefCountedThreadSafe<ThumbnailingAlgorithm> {
 public:
  typedef base::Callback<void(const ThumbnailingContext&, const SkBitmap&)>
      ConsumerCallback;
  // Provides information necessary to crop-and-resize image data from a source
  // canvas of |source_size|. Auxiliary |scale_factor| helps compute the target
  // thumbnail size. Parameters of the required copy operation are assigned to
  // |clipping_rect| (cropping rectangle for the source canvas) and
  // |target_size| (the size of the target bitmap).
  // The return value indicates the type of clipping that will be done.
  virtual ClipResult GetCanvasCopyInfo(const gfx::Size& source_size,
                                       ui::ScaleFactor scale_factor,
                                       gfx::Rect* clipping_rect,
                                       gfx::Size* target_size) const = 0;

  // Invoked to produce a thumbnail image from a |bitmap| extracted by the
  // callee from source canvas according to instructions provided by a call
  // to GetCanvasCopyInfo.
  // Note that ProcessBitmap must be able to handle bitmaps which might have not
  // been processed (scalled/cropped) as requested. |context| gives additional
  // information on the source, including if and how it was clipped.
  // The function shall invoke |callback| once done, passing in fully populated
  // |context| along with resulting thumbnail bitmap.
  virtual void ProcessBitmap(scoped_refptr<ThumbnailingContext> context,
                             const ConsumerCallback& callback,
                             const SkBitmap& bitmap) = 0;

 protected:
  virtual ~ThumbnailingAlgorithm() {}
  friend class base::RefCountedThreadSafe<ThumbnailingAlgorithm>;
};

}

#endif  // CHROME_BROWSER_THUMBNAILS_THUMBNAILING_ALGORITHM_H_
