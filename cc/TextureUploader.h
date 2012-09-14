// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TextureUploader_h
#define TextureUploader_h

#include "LayerTextureUpdater.h"

namespace cc {

class TextureUploader {
public:
    struct Parameters {
        LayerTextureUpdater::Texture* texture;
        IntRect sourceRect;
        IntSize destOffset;
    };

    virtual ~TextureUploader() { }

    virtual bool isBusy() = 0;
    virtual void beginUploads() = 0;
    virtual void endUploads() = 0;
    virtual void uploadTexture(CCResourceProvider*, Parameters) = 0;
};

}

#endif
