// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PICTURE_PILE_IMPL_H_
#define CC_PICTURE_PILE_IMPL_H_

#include <list>
#include <map>

#include "base/threading/thread.h"
#include "cc/cc_export.h"
#include "cc/picture_pile_base.h"
#include "skia/ext/refptr.h"
#include "third_party/skia/include/core/SkPicture.h"

namespace cc {
struct RenderingStats;

class CC_EXPORT PicturePileImpl : public PicturePileBase {
 public:
  static scoped_refptr<PicturePileImpl> Create();

  // Get paint-safe version of this picture for a specific thread.
  PicturePileImpl* GetCloneForDrawingOnThread(base::Thread*);

  // Clone a paint-safe version of this picture.
  scoped_refptr<PicturePileImpl> CloneForDrawing() const;

  // Raster a subrect of this PicturePileImpl into the given canvas.
  // It's only safe to call paint on a cloned version.
  // It is assumed that contentsScale has already been applied to this canvas.
  void Raster(
      SkCanvas* canvas,
      gfx::Rect content_rect,
      float contents_scale,
      RenderingStats* stats);

  void GatherPixelRefs(
      gfx::Rect content_rect,
      float contents_scale,
      std::list<skia::LazyPixelRef*>& pixel_refs);

  void PushPropertiesTo(PicturePileImpl* other);

  skia::RefPtr<SkPicture> GetFlattenedPicture();

 protected:
  friend class PicturePile;

  PicturePileImpl();
  ~PicturePileImpl();

  typedef std::map<base::PlatformThreadId, scoped_refptr<PicturePileImpl> >
      CloneMap;
  CloneMap clones_;

  DISALLOW_COPY_AND_ASSIGN(PicturePileImpl);
};

}  // namespace cc

#endif  // CC_PICTURE_PILE_IMPL_H_
