// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PICTURE_PILE_H_
#define CC_PICTURE_PILE_H_

#include "cc/picture_pile_base.h"
#include "ui/gfx/rect.h"

namespace cc {
class PicturePileImpl;
class Region;
struct RenderingStats;

class CC_EXPORT PicturePile : public PicturePileBase {
 public:
  PicturePile();

  // Re-record parts of the picture that are invalid.
  // Invalidations are in layer space.
  void Update(
      ContentLayerClient* painter,
      const Region& invalidation,
      RenderingStats& stats);

  // Update other with a shallow copy of this (main => compositor thread commit)
  void PushPropertiesTo(PicturePileImpl* other);

 private:
  ~PicturePile();
  friend class PicturePileImpl;

  void InvalidateRect(gfx::Rect invalidation);
  void ResetPile(ContentLayerClient* painter, RenderingStats& stats);

  DISALLOW_COPY_AND_ASSIGN(PicturePile);
};

}  // namespace cc

#endif  // CC_PICTURE_PILE_H_
