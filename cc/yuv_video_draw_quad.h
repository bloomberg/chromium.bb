// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCYUVVideoDrawQuad_h
#define CCYUVVideoDrawQuad_h

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "CCDrawQuad.h"
#include "CCVideoLayerImpl.h"

namespace cc {

class YUVVideoDrawQuad : public DrawQuad {
public:
    static scoped_ptr<YUVVideoDrawQuad> create(const SharedQuadState*, const IntRect&, const VideoLayerImpl::FramePlane& yPlane, const VideoLayerImpl::FramePlane& uPlane, const VideoLayerImpl::FramePlane& vPlane);

    const VideoLayerImpl::FramePlane& yPlane() const { return m_yPlane; }
    const VideoLayerImpl::FramePlane& uPlane() const { return m_uPlane; }
    const VideoLayerImpl::FramePlane& vPlane() const { return m_vPlane; }

    static const YUVVideoDrawQuad* materialCast(const DrawQuad*);
private:
    YUVVideoDrawQuad(const SharedQuadState*, const IntRect&, const VideoLayerImpl::FramePlane& yPlane, const VideoLayerImpl::FramePlane& uPlane, const VideoLayerImpl::FramePlane& vPlane);

    VideoLayerImpl::FramePlane m_yPlane;
    VideoLayerImpl::FramePlane m_uPlane;
    VideoLayerImpl::FramePlane m_vPlane;

    DISALLOW_COPY_AND_ASSIGN(YUVVideoDrawQuad);
};

}

#endif
