// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TextureUploader_h
#define TextureUploader_h

#include "IntRect.h"

class SkBitmap;
class SkPicture;

namespace cc {

class CCPrioritizedTexture;
class CCResourceProvider;

class TextureUploader {
public:
    struct Geometry {
        IntRect contentRect;
        IntRect sourceRect;
        IntSize destOffset;
    };
    struct Parameters {
        CCPrioritizedTexture* texture;
        const SkBitmap* bitmap;
        SkPicture* picture;
        Geometry geometry;
    };

    virtual ~TextureUploader() { }

    virtual size_t numBlockingUploads() = 0;
    virtual void markPendingUploadsAsNonBlocking() = 0;

    // Returns our throughput on the GPU process
    virtual double estimatedTexturesPerSecond() = 0;
    virtual void uploadTexture(CCResourceProvider*, Parameters) = 0;
};

}

#endif
