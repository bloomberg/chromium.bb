// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCE_UPDATE_H_
#define CC_RESOURCE_UPDATE_H_

#include "ui/gfx/rect.h"
#include "ui/gfx/vector2d.h"

class SkBitmap;
class SkPicture;

namespace cc {

class PrioritizedTexture;

struct ResourceUpdate {
    static ResourceUpdate Create(PrioritizedTexture*,
                                 const SkBitmap*,
                                 gfx::Rect content_rect,
                                 gfx::Rect source_rect,
                                 gfx::Vector2d dest_offset);
    static ResourceUpdate CreateFromPicture(PrioritizedTexture*,
                                            SkPicture*,
                                            gfx::Rect content_rect,
                                            gfx::Rect source_rect,
                                            gfx::Vector2d dest_offset);

    ResourceUpdate();
    virtual ~ResourceUpdate();

    PrioritizedTexture* texture;
    const SkBitmap* bitmap;
    SkPicture* picture;
    gfx::Rect content_rect;
    gfx::Rect source_rect;
    gfx::Vector2d dest_offset;
};

}

#endif  // CC_RESOURCE_UPDATE_H_
