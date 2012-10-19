// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "cc/resource_update.h"

#include "base/logging.h"

namespace cc {

ResourceUpdate ResourceUpdate::Create(CCPrioritizedTexture* texture,
                                      const SkBitmap* bitmap,
                                      IntRect content_rect,
                                      IntRect source_rect,
                                      IntSize dest_offset) {
    CHECK(content_rect.contains(source_rect));
    ResourceUpdate update;
    update.texture = texture;
    update.bitmap = bitmap;
    update.content_rect = content_rect;
    update.source_rect = source_rect;
    update.dest_offset = dest_offset;
    return update;
}

ResourceUpdate ResourceUpdate::CreateFromPicture(CCPrioritizedTexture* texture,
                                                 SkPicture* picture,
                                                 IntRect content_rect,
                                                 IntRect source_rect,
                                                 IntSize dest_offset) {
    CHECK(content_rect.contains(source_rect));
    ResourceUpdate update;
    update.texture = texture;
    update.picture = picture;
    update.content_rect = content_rect;
    update.source_rect = source_rect;
    update.dest_offset = dest_offset;
    return update;
}

ResourceUpdate::ResourceUpdate()
    : texture(NULL),
      bitmap(NULL),
      picture(NULL) {
}

ResourceUpdate::~ResourceUpdate() {
}

}  // namespace cc
