// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/yuv_video_draw_quad.h"

#include "base/logging.h"

namespace cc {

scoped_ptr<YUVVideoDrawQuad> YUVVideoDrawQuad::create(
    const SharedQuadState* sharedQuadState,
    const gfx::Rect& quadRect,
    const gfx::Rect& opaqueRect,
    const gfx::SizeF& texScale,
    const VideoLayerImpl::FramePlane& yPlane,
    const VideoLayerImpl::FramePlane& uPlane,
    const VideoLayerImpl::FramePlane& vPlane)
{
    return make_scoped_ptr(new YUVVideoDrawQuad(sharedQuadState,
                                                quadRect, opaqueRect, texScale,
                                                yPlane, uPlane, vPlane));
}

YUVVideoDrawQuad::YUVVideoDrawQuad(
    const SharedQuadState* sharedQuadState,
    const gfx::Rect& quadRect,
    const gfx::Rect& opaqueRect,
    const gfx::SizeF& texScale,
    const VideoLayerImpl::FramePlane& yPlane,
    const VideoLayerImpl::FramePlane& uPlane,
    const VideoLayerImpl::FramePlane& vPlane)
    : DrawQuad(sharedQuadState, DrawQuad::YUV_VIDEO_CONTENT, quadRect, opaqueRect)
    , m_texScale(texScale)
    , m_yPlane(yPlane)
    , m_uPlane(uPlane)
    , m_vPlane(vPlane)
{
}

YUVVideoDrawQuad::~YUVVideoDrawQuad()
{
}

const YUVVideoDrawQuad* YUVVideoDrawQuad::materialCast(const DrawQuad* quad)
{
    DCHECK(quad->material() == DrawQuad::YUV_VIDEO_CONTENT);
    return static_cast<const YUVVideoDrawQuad*>(quad);
}

}  // namespace cc
