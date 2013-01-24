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
      gfx::Rect visible_layer_rect,
      RenderingStats& stats);

  // Update other with a shallow copy of this (main => compositor thread commit)
  void PushPropertiesTo(PicturePileImpl* other);

 private:
  ~PicturePile();
  friend class PicturePileImpl;

  // Add an invalidation to this picture list.  If the list needs to be
  // entirely recreated, leave it empty.  Do not call this on an empty list.
  void InvalidateRect(
      PictureList& picture_list,
      gfx::Rect invalidation);

  DISALLOW_COPY_AND_ASSIGN(PicturePile);
};

}  // namespace cc

#endif  // CC_PICTURE_PILE_H_
