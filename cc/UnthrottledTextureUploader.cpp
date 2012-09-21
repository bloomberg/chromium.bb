// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "UnthrottledTextureUploader.h"

namespace cc {

bool UnthrottledTextureUploader::isBusy()
{
    return false;
}

double UnthrottledTextureUploader::estimatedTexturesPerSecond()
{
    return std::numeric_limits<double>::max();
}

void UnthrottledTextureUploader::uploadTexture(CCResourceProvider* resourceProvider, Parameters upload)
{
    upload.texture->updateRect(resourceProvider, upload.sourceRect, upload.destOffset);
}

}
