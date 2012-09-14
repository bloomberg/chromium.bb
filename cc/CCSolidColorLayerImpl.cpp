// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#if USE(ACCELERATED_COMPOSITING)

#include "CCSolidColorLayerImpl.h"

#include "CCQuadSink.h"
#include "CCSolidColorDrawQuad.h"
#include <wtf/MathExtras.h>

using namespace std;
using WebKit::WebTransformationMatrix;

namespace cc {

CCSolidColorLayerImpl::CCSolidColorLayerImpl(int id)
    : CCLayerImpl(id)
    , m_tileSize(256)
{
}

CCSolidColorLayerImpl::~CCSolidColorLayerImpl()
{
}

void CCSolidColorLayerImpl::appendQuads(CCQuadSink& quadSink, CCAppendQuadsData& appendQuadsData)
{
    CCSharedQuadState* sharedQuadState = quadSink.useSharedQuadState(createSharedQuadState());
    appendDebugBorderQuad(quadSink, sharedQuadState, appendQuadsData);

    // We create a series of smaller quads instead of just one large one so that the
    // culler can reduce the total pixels drawn.
    int width = contentBounds().width();
    int height = contentBounds().height();
    for (int x = 0; x < width; x += m_tileSize) {
        for (int y = 0; y < height; y += m_tileSize) {
            IntRect solidTileRect(x, y, min(width - x, m_tileSize), min(height - y, m_tileSize));
            quadSink.append(CCSolidColorDrawQuad::create(sharedQuadState, solidTileRect, backgroundColor()), appendQuadsData);
        }
    }
}

} // namespace cc
#endif // USE(ACCELERATED_COMPOSITING)
