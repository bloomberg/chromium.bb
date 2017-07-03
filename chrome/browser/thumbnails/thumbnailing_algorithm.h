// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_THUMBNAILS_THUMBNAILING_ALGORITHM_H_
#define CHROME_BROWSER_THUMBNAILS_THUMBNAILING_ALGORITHM_H_

#include "base/memory/ref_counted.h"
#include "chrome/browser/thumbnails/thumbnailing_context.h"
#include "ui/base/resource/scale_factor.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace thumbnails {

// An interface abstracting thumbnailing algorithms. Instances are intended to
// be created by ThumbnailService's implementations and used by
// ThumbnailTabHelper as consumers of captured source images.
class ThumbnailingAlgorithm
    : public base::RefCountedThreadSafe<ThumbnailingAlgorithm> {
 public:
  // Provides information necessary to crop-and-resize image data from a source
  // canvas of |source_size|. Auxiliary |scale_factor| helps compute the target
  // thumbnail size to be copied from the backing store, in pixels. Parameters
  // of the required copy operation are assigned to |clipping_rect| (cropping
  // rectangle for the source canvas) and |copy_size| (the size of the copied
  // bitmap in pixels). The return value indicates the type of clipping that
  // will be done.
  virtual ClipResult GetCanvasCopyInfo(const gfx::Size& source_size,
                                       ui::ScaleFactor scale_factor,
                                       gfx::Rect* clipping_rect,
                                       gfx::Size* copy_size) const = 0;

 protected:
  virtual ~ThumbnailingAlgorithm() {}
  friend class base::RefCountedThreadSafe<ThumbnailingAlgorithm>;
};

}  // namespace thumbnails

#endif  // CHROME_BROWSER_THUMBNAILS_THUMBNAILING_ALGORITHM_H_
