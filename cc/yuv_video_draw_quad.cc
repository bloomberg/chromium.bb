// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "CCYUVVideoDrawQuad.h"

#include "base/logging.h"

namespace cc {

scoped_ptr<CCYUVVideoDrawQuad> CCYUVVideoDrawQuad::create(const CCSharedQuadState* sharedQuadState, const gfx::Rect& quadRect, const CCVideoLayerImpl::FramePlane& yPlane, const CCVideoLayerImpl::FramePlane& uPlane, const CCVideoLayerImpl::FramePlane& vPlane)
{
    return make_scoped_ptr(new CCYUVVideoDrawQuad(sharedQuadState, quadRect, yPlane, uPlane, vPlane));
}

CCYUVVideoDrawQuad::CCYUVVideoDrawQuad(const CCSharedQuadState* sharedQuadState, const gfx::Rect& quadRect, const CCVideoLayerImpl::FramePlane& yPlane, const CCVideoLayerImpl::FramePlane& uPlane, const CCVideoLayerImpl::FramePlane& vPlane)
    : CCDrawQuad(sharedQuadState, CCDrawQuad::YUVVideoContent, quadRect)
    , m_yPlane(yPlane)
    , m_uPlane(uPlane)
    , m_vPlane(vPlane)
{
}

const CCYUVVideoDrawQuad* CCYUVVideoDrawQuad::materialCast(const CCDrawQuad* quad)
{
    DCHECK(quad->material() == CCDrawQuad::YUVVideoContent);
    return static_cast<const CCYUVVideoDrawQuad*>(quad);
}

}  // namespace cc
