// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/heads_up_display_layer.h"

#include "base/debug/trace_event.h"
#include "cc/heads_up_display_layer_impl.h"
#include "cc/layer_tree_host.h"

namespace cc {

scoped_refptr<HeadsUpDisplayLayer> HeadsUpDisplayLayer::Create()
{
    return make_scoped_refptr(new HeadsUpDisplayLayer());
}

HeadsUpDisplayLayer::HeadsUpDisplayLayer()
    : Layer()
{
    SetBounds(gfx::Size(256, 256));
}

HeadsUpDisplayLayer::~HeadsUpDisplayLayer()
{
}

void HeadsUpDisplayLayer::Update(ResourceUpdateQueue*, const OcclusionTracker*, RenderingStats*)
{
    const LayerTreeDebugState& debugState = layer_tree_host()->debug_state();
    int maxTextureSize = layer_tree_host()->GetRendererCapabilities().max_texture_size;

    int deviceViewportInLayoutPixelsWidth = layer_tree_host()->device_viewport_size().width() / layer_tree_host()->device_scale_factor();
    int deviceViewportInLayoutPixelsHeight = layer_tree_host()->device_viewport_size().height() / layer_tree_host()->device_scale_factor();

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

    SetBounds(bounds);
    SetTransform(matrix);
}

bool HeadsUpDisplayLayer::DrawsContent() const
{
    return true;
}

scoped_ptr<LayerImpl> HeadsUpDisplayLayer::CreateLayerImpl(LayerTreeImpl* treeImpl)
{
    return HeadsUpDisplayLayerImpl::Create(treeImpl, layer_id_).PassAs<LayerImpl>();
}

}  // namespace cc
