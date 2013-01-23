// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/solid_color_layer_impl.h"

#include "cc/quad_sink.h"
#include "cc/solid_color_draw_quad.h"

namespace cc {

SolidColorLayerImpl::SolidColorLayerImpl(LayerTreeImpl* treeImpl, int id)
    : LayerImpl(treeImpl, id)
    , m_tileSize(256)
{
}

SolidColorLayerImpl::~SolidColorLayerImpl()
{
}

scoped_ptr<LayerImpl> SolidColorLayerImpl::createLayerImpl(LayerTreeImpl* treeImpl)
{
    return SolidColorLayerImpl::create(treeImpl, id()).PassAs<LayerImpl>();
}

void SolidColorLayerImpl::appendQuads(QuadSink& quadSink, AppendQuadsData& appendQuadsData)
{
    SharedQuadState* sharedQuadState = quadSink.useSharedQuadState(createSharedQuadState());
    appendDebugBorderQuad(quadSink, sharedQuadState, appendQuadsData);

    // We create a series of smaller quads instead of just one large one so that the
    // culler can reduce the total pixels drawn.
    int width = contentBounds().width();
    int height = contentBounds().height();
    for (int x = 0; x < width; x += m_tileSize) {
        for (int y = 0; y < height; y += m_tileSize) {
            gfx::Rect solidTileRect(x, y, std::min(width - x, m_tileSize), std::min(height - y, m_tileSize));
            scoped_ptr<SolidColorDrawQuad> quad = SolidColorDrawQuad::Create();
            quad->SetNew(sharedQuadState, solidTileRect, backgroundColor());
            quadSink.append(quad.PassAs<DrawQuad>(), appendQuadsData);
        }
    }
}

const char* SolidColorLayerImpl::layerTypeAsString() const
{
    return "SolidColorLayer";
}

}  // namespace cc
