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

void UnthrottledTextureUploader::uploadTexture(CCResourceProvider* resourceProvider, Parameters upload)
{
    if (upload.bitmap) {
        upload.bitmap->lockPixels();
        upload.texture->upload(
            resourceProvider,
            static_cast<const uint8_t*>(upload.bitmap->getPixels()),
            upload.geometry.contentRect,
            upload.geometry.sourceRect,
            upload.geometry.destOffset);
        upload.bitmap->unlockPixels();
    }

    ASSERT(!upload.picture);
}

}
