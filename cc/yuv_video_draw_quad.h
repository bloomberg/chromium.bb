// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_YUV_VIDEO_DRAW_QUAD_H_
#define CC_YUV_VIDEO_DRAW_QUAD_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "cc/cc_export.h"
#include "cc/draw_quad.h"
#include "cc/video_layer_impl.h"

namespace cc {

class CC_EXPORT YUVVideoDrawQuad : public DrawQuad {
public:
    static scoped_ptr<YUVVideoDrawQuad> create(
        const SharedQuadState* sharedQuadState,
        const gfx::Rect& quadRect,
        const gfx::Rect& opaqueRect,
        const gfx::SizeF& texScale,
        const VideoLayerImpl::FramePlane& yPlane,
        const VideoLayerImpl::FramePlane& uPlane,
        const VideoLayerImpl::FramePlane& vPlane);

    ~YUVVideoDrawQuad();

    const gfx::SizeF& texScale() const { return m_texScale; }

    const VideoLayerImpl::FramePlane& yPlane() const { return m_yPlane; }
    const VideoLayerImpl::FramePlane& uPlane() const { return m_uPlane; }
    const VideoLayerImpl::FramePlane& vPlane() const { return m_vPlane; }

    static const YUVVideoDrawQuad* materialCast(const DrawQuad*);
private:
    YUVVideoDrawQuad(
        const SharedQuadState* sharedQuadState,
        const gfx::Rect& quadRect,
        const gfx::Rect& opaqueRect,
        const gfx::SizeF& texScale,
        const VideoLayerImpl::FramePlane& yPlane,
        const VideoLayerImpl::FramePlane& uPlane,
        const VideoLayerImpl::FramePlane& vPlane);

    gfx::SizeF m_texScale;
    VideoLayerImpl::FramePlane m_yPlane;
    VideoLayerImpl::FramePlane m_uPlane;
    VideoLayerImpl::FramePlane m_vPlane;
};

}

#endif  // CC_YUV_VIDEO_DRAW_QUAD_H_
