// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCTextureDrawQuad_h
#define CCTextureDrawQuad_h

#include "CCDrawQuad.h"
#include "FloatRect.h"
#include <wtf/PassOwnPtr.h>

namespace cc {

#pragma pack(push, 4)

class CCTextureDrawQuad : public CCDrawQuad {
public:
    static PassOwnPtr<CCTextureDrawQuad> create(const CCSharedQuadState*, const IntRect&, unsigned resourceId, bool premultipliedAlpha, const FloatRect& uvRect, bool flipped);
    FloatRect uvRect() const { return m_uvRect; }

    unsigned resourceId() const { return m_resourceId; }
    bool premultipliedAlpha() const { return  m_premultipliedAlpha; }
    bool flipped() const { return m_flipped; }

    void setNeedsBlending();

    static const CCTextureDrawQuad* materialCast(const CCDrawQuad*);
private:
    CCTextureDrawQuad(const CCSharedQuadState*, const IntRect&, unsigned resourceId, bool premultipliedAlpha, const FloatRect& uvRect, bool flipped);

    unsigned m_resourceId;
    bool m_premultipliedAlpha;
    FloatRect m_uvRect;
    bool m_flipped;
};

#pragma pack(pop)

}

#endif
