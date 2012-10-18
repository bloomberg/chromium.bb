// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "cc/unthrottled_texture_uploader.h"

#include "CCPrioritizedTexture.h"

namespace cc {

size_t UnthrottledTextureUploader::numBlockingUploads()
{
    return 0;
}

void UnthrottledTextureUploader::markPendingUploadsAsNonBlocking()
{
}

double UnthrottledTextureUploader::estimatedTexturesPerSecond()
{
    return std::numeric_limits<double>::max();
}

void UnthrottledTextureUploader::uploadTexture(
    CCResourceProvider* resourceProvider,
    CCPrioritizedTexture* texture,
    const SkBitmap* bitmap,
    IntRect content_rect,
    IntRect source_rect,
    IntSize dest_offset)
{
    if (bitmap) {
        bitmap->lockPixels();
        texture->upload(resourceProvider,
                        static_cast<const uint8_t*>(bitmap->getPixels()),
                        content_rect,
                        source_rect,
                        dest_offset);
        bitmap->unlockPixels();
    }
}

}
