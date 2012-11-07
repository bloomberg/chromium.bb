// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCE_UPDATE_H_
#define CC_RESOURCE_UPDATE_H_

#include "ui/gfx/rect.h"
#include "ui/gfx/vector2d.h"
#include "cc/cc_export.h"

class SkBitmap;
class SkPicture;

namespace cc {

class PrioritizedResource;

struct CC_EXPORT ResourceUpdate {
    static ResourceUpdate Create(PrioritizedResource*,
                                 const SkBitmap*,
                                 gfx::Rect content_rect,
                                 gfx::Rect source_rect,
                                 gfx::Vector2d dest_offset);
    static ResourceUpdate CreateFromPicture(PrioritizedResource*,
                                            SkPicture*,
                                            gfx::Rect content_rect,
                                            gfx::Rect source_rect,
                                            gfx::Vector2d dest_offset);

    ResourceUpdate();
    virtual ~ResourceUpdate();

    PrioritizedResource* texture;
    const SkBitmap* bitmap;
    SkPicture* picture;
    gfx::Rect content_rect;
    gfx::Rect source_rect;
    gfx::Vector2d dest_offset;
};

}

#endif  // CC_RESOURCE_UPDATE_H_
