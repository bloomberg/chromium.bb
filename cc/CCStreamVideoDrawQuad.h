// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCStreamVideoDrawQuad_h
#define CCStreamVideoDrawQuad_h

#include "CCDrawQuad.h"
#include <public/WebTransformationMatrix.h>

#include <wtf/PassOwnPtr.h>

namespace cc {

#pragma pack(push, 4)

class CCStreamVideoDrawQuad : public CCDrawQuad {
public:
    static PassOwnPtr<CCStreamVideoDrawQuad> create(const CCSharedQuadState*, const IntRect&, unsigned textureId, const WebKit::WebTransformationMatrix&);

    unsigned textureId() const { return m_textureId; }
    const WebKit::WebTransformationMatrix& matrix() const { return m_matrix; }

    static const CCStreamVideoDrawQuad* materialCast(const CCDrawQuad*);
private:
    CCStreamVideoDrawQuad(const CCSharedQuadState*, const IntRect&, unsigned textureId, const WebKit::WebTransformationMatrix&);

    unsigned m_textureId;
    WebKit::WebTransformationMatrix m_matrix;
};

#pragma pack(pop)

}

#endif
