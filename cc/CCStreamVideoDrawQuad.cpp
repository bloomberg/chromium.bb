// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "CCStreamVideoDrawQuad.h"

namespace cc {

PassOwnPtr<CCStreamVideoDrawQuad> CCStreamVideoDrawQuad::create(const CCSharedQuadState* sharedQuadState, const IntRect& quadRect, unsigned textureId, const WebKit::WebTransformationMatrix& matrix)
{
    return adoptPtr(new CCStreamVideoDrawQuad(sharedQuadState, quadRect, textureId, matrix));
}

CCStreamVideoDrawQuad::CCStreamVideoDrawQuad(const CCSharedQuadState* sharedQuadState, const IntRect& quadRect, unsigned textureId, const WebKit::WebTransformationMatrix& matrix)
    : CCDrawQuad(sharedQuadState, CCDrawQuad::StreamVideoContent, quadRect)
    , m_textureId(textureId)
    , m_matrix(matrix)
{
}

const CCStreamVideoDrawQuad* CCStreamVideoDrawQuad::materialCast(const CCDrawQuad* quad)
{
    ASSERT(quad->material() == CCDrawQuad::StreamVideoContent);
    return static_cast<const CCStreamVideoDrawQuad*>(quad);
}

}
