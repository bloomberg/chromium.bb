// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "CCIOSurfaceDrawQuad.h"

namespace cc {

scoped_ptr<CCIOSurfaceDrawQuad> CCIOSurfaceDrawQuad::create(const CCSharedQuadState* sharedQuadState, const IntRect& quadRect, const IntSize& ioSurfaceSize, unsigned ioSurfaceTextureId, Orientation orientation)
{
    return scoped_ptr<CCIOSurfaceDrawQuad>(new CCIOSurfaceDrawQuad(sharedQuadState, quadRect, ioSurfaceSize, ioSurfaceTextureId, orientation));
}

CCIOSurfaceDrawQuad::CCIOSurfaceDrawQuad(const CCSharedQuadState* sharedQuadState, const IntRect& quadRect, const IntSize& ioSurfaceSize, unsigned ioSurfaceTextureId, Orientation orientation)
    : CCDrawQuad(sharedQuadState, CCDrawQuad::IOSurfaceContent, quadRect)
    , m_ioSurfaceSize(ioSurfaceSize)
    , m_ioSurfaceTextureId(ioSurfaceTextureId)
    , m_orientation(orientation)
{
}

const CCIOSurfaceDrawQuad* CCIOSurfaceDrawQuad::materialCast(const CCDrawQuad* quad)
{
    ASSERT(quad->material() == CCDrawQuad::IOSurfaceContent);
    return static_cast<const CCIOSurfaceDrawQuad*>(quad);
}

}
