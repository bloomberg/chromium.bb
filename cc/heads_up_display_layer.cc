// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/heads_up_display_layer.h"

#include "base/debug/trace_event.h"
#include "cc/heads_up_display_layer_impl.h"
#include "cc/layer_tree_host.h"

namespace cc {

scoped_refptr<HeadsUpDisplayLayer> HeadsUpDisplayLayer::create()
{
    return make_scoped_refptr(new HeadsUpDisplayLayer());
}

HeadsUpDisplayLayer::HeadsUpDisplayLayer()
    : Layer()
{
    setBounds(gfx::Size(256, 256));
}

HeadsUpDisplayLayer::~HeadsUpDisplayLayer()
{
}

void HeadsUpDisplayLayer::update(ResourceUpdateQueue&, const OcclusionTracker*, RenderingStats*)
{
    const LayerTreeDebugState& debugState = layerTreeHost()->debugState();
    int maxTextureSize = layerTreeHost()->rendererCapabilities().maxTextureSize;

    int deviceViewportInLayoutPixelsWidth = layerTreeHost()->deviceViewportSize().width() / layerTreeHost()->deviceScaleFactor();
    int deviceViewportInLayoutPixelsHeight = layerTreeHost()->deviceViewportSize().height() / layerTreeHost()->deviceScaleFactor();

    gfx::Size bounds;
    gfx::Transform matrix;
    matrix.MakeIdentity();

    if (debugState.showPlatformLayerTree || debugState.showHudRects()) {
        int width = std::min(maxTextureSize, deviceViewportInLayoutPixelsWidth);
        int height = std::min(maxTextureSize, deviceViewportInLayoutPixelsHeight);
        bounds = gfx::Size(width, height);
    } else {
        bounds = gfx::Size(256, 256);
        matrix.Translate(deviceViewportInLayoutPixelsWidth - 256, 0);
    }

    setBounds(bounds);
    setTransform(matrix);
}

bool HeadsUpDisplayLayer::drawsContent() const
{
    return true;
}

scoped_ptr<LayerImpl> HeadsUpDisplayLayer::createLayerImpl(LayerTreeImpl* treeImpl)
{
    return HeadsUpDisplayLayerImpl::create(treeImpl, m_layerId).PassAs<LayerImpl>();
}

}  // namespace cc
