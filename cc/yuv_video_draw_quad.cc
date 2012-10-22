// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "cc/yuv_video_draw_quad.h"

#include "base/logging.h"

namespace cc {

scoped_ptr<YUVVideoDrawQuad> YUVVideoDrawQuad::create(const SharedQuadState* sharedQuadState, const gfx::Rect& quadRect, const VideoLayerImpl::FramePlane& yPlane, const VideoLayerImpl::FramePlane& uPlane, const VideoLayerImpl::FramePlane& vPlane)
{
    return make_scoped_ptr(new YUVVideoDrawQuad(sharedQuadState, quadRect, yPlane, uPlane, vPlane));
}

YUVVideoDrawQuad::YUVVideoDrawQuad(const SharedQuadState* sharedQuadState, const gfx::Rect& quadRect, const VideoLayerImpl::FramePlane& yPlane, const VideoLayerImpl::FramePlane& uPlane, const VideoLayerImpl::FramePlane& vPlane)
    : DrawQuad(sharedQuadState, DrawQuad::YUVVideoContent, quadRect)
    , m_yPlane(yPlane)
    , m_uPlane(uPlane)
    , m_vPlane(vPlane)
{
}

const YUVVideoDrawQuad* YUVVideoDrawQuad::materialCast(const DrawQuad* quad)
{
    DCHECK(quad->material() == DrawQuad::YUVVideoContent);
    return static_cast<const YUVVideoDrawQuad*>(quad);
}

}  // namespace cc
