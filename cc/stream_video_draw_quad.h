// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_STREAM_VIDEO_DRAW_QUAD_H_
#define CC_STREAM_VIDEO_DRAW_QUAD_H_

#include "base/memory/scoped_ptr.h"
#include "cc/cc_export.h"
#include "cc/draw_quad.h"
#include <public/WebTransformationMatrix.h>

namespace cc {

class CC_EXPORT StreamVideoDrawQuad : public DrawQuad {
public:
    static scoped_ptr<StreamVideoDrawQuad> create(const SharedQuadState*, const gfx::Rect&, const gfx::Rect& opaqueRect, unsigned textureId, const WebKit::WebTransformationMatrix&);

    unsigned textureId() const { return m_textureId; }
    const WebKit::WebTransformationMatrix& matrix() const { return m_matrix; }

    static const StreamVideoDrawQuad* materialCast(const DrawQuad*);
private:
    StreamVideoDrawQuad(const SharedQuadState*, const gfx::Rect&, const gfx::Rect& opaqueRect, unsigned textureId, const WebKit::WebTransformationMatrix&);

    unsigned m_textureId;
    WebKit::WebTransformationMatrix m_matrix;
};

}

#endif  // CC_STREAM_VIDEO_DRAW_QUAD_H_
