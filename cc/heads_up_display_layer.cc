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
    , m_showFPSCounter(false)
{
    setBounds(gfx::Size(256, 128));
}

HeadsUpDisplayLayer::~HeadsUpDisplayLayer()
{
}

void HeadsUpDisplayLayer::update(ResourceUpdateQueue&, const OcclusionTracker*, RenderingStats&)
{
    const LayerTreeSettings& settings = layerTreeHost()->settings();
    int maxTextureSize = layerTreeHost()->rendererCapabilities().maxTextureSize;

    gfx::Size bounds;
    WebKit::WebTransformationMatrix matrix;
    matrix.makeIdentity();

    if (settings.showPlatformLayerTree || settings.showDebugRects()) {
        int width = std::min(maxTextureSize, layerTreeHost()->deviceViewportSize().width());
        int height = std::min(maxTextureSize, layerTreeHost()->deviceViewportSize().height());
        bounds = gfx::Size(width, height);
    } else {
        bounds = gfx::Size(256, 128);
        matrix.translate(layerTreeHost()->deviceViewportSize().width() - 256, 0);
    }

    setBounds(bounds);
    setTransform(matrix);
}

bool HeadsUpDisplayLayer::drawsContent() const
{
    return true;
}

void HeadsUpDisplayLayer::setFontAtlas(scoped_ptr<FontAtlas> fontAtlas)
{
    m_fontAtlas = fontAtlas.Pass();
    setNeedsCommit();
}

void HeadsUpDisplayLayer::setShowFPSCounter(bool show)
{
    m_showFPSCounter = show;
    setNeedsCommit();
}

scoped_ptr<LayerImpl> HeadsUpDisplayLayer::createLayerImpl()
{
    return HeadsUpDisplayLayerImpl::create(m_layerId).PassAs<LayerImpl>();
}

void HeadsUpDisplayLayer::pushPropertiesTo(LayerImpl* layerImpl)
{
    Layer::pushPropertiesTo(layerImpl);

    HeadsUpDisplayLayerImpl* hudLayerImpl = static_cast<HeadsUpDisplayLayerImpl*>(layerImpl);
    hudLayerImpl->setShowFPSCounter(m_showFPSCounter);

    if (m_fontAtlas.get())
        hudLayerImpl->setFontAtlas(m_fontAtlas.Pass());
}

}  // namespace cc
