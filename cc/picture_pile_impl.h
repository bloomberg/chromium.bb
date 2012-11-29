// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PICTURE_PILE_IMPL_H_
#define CC_PICTURE_PILE_IMPL_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "cc/cc_export.h"
#include "cc/picture.h"
#include "cc/scoped_ptr_vector.h"
#include "ui/gfx/rect.h"

namespace cc {
class PicturePile;

class CC_EXPORT PicturePileImpl : public base::RefCounted<PicturePileImpl> {
public:
  static scoped_refptr<PicturePileImpl> Create();

  // Clone a paint-safe version of this picture.
  scoped_refptr<PicturePileImpl> CloneForDrawing() const;

  // Raster a subrect of this PicturePileImpl into the given canvas.
  // It's only safe to call paint on a cloned version.
  // It is assumed that contentsScale has already been applied to this canvas.
  void Raster(SkCanvas* canvas, gfx::Rect rect);

private:
  friend class PicturePile;

  PicturePileImpl();
  ~PicturePileImpl();

  std::vector<scoped_refptr<Picture> > pile_;

  friend class base::RefCounted<PicturePileImpl>;
  DISALLOW_COPY_AND_ASSIGN(PicturePileImpl);
};

}  // namespace cc

#endif  // CC_PICTURE_PILE_IMPL_H_
