// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCE_UPDATE_H_
#define CC_RESOURCE_UPDATE_H_

#include "IntRect.h"

class SkBitmap;
class SkPicture;

namespace cc {

class PrioritizedTexture;

struct ResourceUpdate {
    static ResourceUpdate Create(PrioritizedTexture*,
                                 const SkBitmap*,
                                 IntRect content_rect,
                                 IntRect source_rect,
                                 IntSize dest_offset);
    static ResourceUpdate CreateFromPicture(PrioritizedTexture*,
                                            SkPicture*,
                                            IntRect content_rect,
                                            IntRect source_rect,
                                            IntSize dest_offset);

    ResourceUpdate();
    virtual ~ResourceUpdate();

    PrioritizedTexture* texture;
    const SkBitmap* bitmap;
    SkPicture* picture;
    IntRect content_rect;
    IntRect source_rect;
    IntSize dest_offset;
};

}

#endif  // CC_RESOURCE_UPDATE_H_
