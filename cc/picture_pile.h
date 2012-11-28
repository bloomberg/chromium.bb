// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PICTURE_PILE_H_
#define CC_PICTURE_PILE_H_

#include "base/basictypes.h"
#include "cc/cc_export.h"
#include "cc/picture.h"
#include "cc/region.h"
#include "cc/scoped_ptr_vector.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"

namespace cc {

struct RenderingStats;

class CC_EXPORT PicturePile {
public:
  PicturePile();
  ~PicturePile();

  // Resize the PicturePile, invalidating / dropping recorded pictures as
  // necessary.
  void Resize(gfx::Size);
  gfx::Size size() const { return size_; }

  // Re-record parts of the picture that are invalid.
  // Invalidations are in layer space.
  void Update(
      ContentLayerClient* painter,
      const Region& invalidation,
      RenderingStats& stats);

  // Update other with a shallow copy of this (main => compositor thread commit)
  void PushPropertiesTo(PicturePile& other);

  // Clone a paint-safe version of this picture (with cloned PicturePileRecords)
  scoped_ptr<PicturePile> CloneForDrawing() const;

  // Raster a subrect of this PicturePile into the given canvas.
  // It's only safe to call paint on a cloned version.
  // It is assumed that contentsScale has already been applied to this canvas.
  void Raster(SkCanvas* canvas, gfx::Rect rect);

private:
  std::vector<scoped_refptr<Picture> > pile_;
  gfx::Size size_;

  DISALLOW_COPY_AND_ASSIGN(PicturePile);
};

}  // namespace cc

#endif  // CC_PICTURE_PILE_H_
