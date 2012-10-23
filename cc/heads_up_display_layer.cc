// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

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

    setBounds(IntSize(512, 128));
}

HeadsUpDisplayLayer::~HeadsUpDisplayLayer()
{
}

void HeadsUpDisplayLayer::update(TextureUpdateQueue&, const OcclusionTracker*, RenderingStats&)
{
    const LayerTreeSettings& settings = layerTreeHost()->settings();
    int maxTextureSize = layerTreeHost()->rendererCapabilities().maxTextureSize;

    IntSize bounds;
    if (settings.showPlatformLayerTree || settings.showDebugRects()) {
        bounds.setWidth(std::min(maxTextureSize, layerTreeHost()->deviceViewportSize().width()));
        bounds.setHeight(std::min(maxTextureSize, layerTreeHost()->deviceViewportSize().height()));
    } else {
        bounds.setWidth(512);
        bounds.setHeight(128);
    }

    setBounds(bounds);
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

scoped_ptr<LayerImpl> HeadsUpDisplayLayer::createLayerImpl()
{
    return HeadsUpDisplayLayerImpl::create(m_layerId).PassAs<LayerImpl>();
}

void HeadsUpDisplayLayer::pushPropertiesTo(LayerImpl* layerImpl)
{
    Layer::pushPropertiesTo(layerImpl);

    if (!m_fontAtlas.get())
        return;

    HeadsUpDisplayLayerImpl* hudLayerImpl = static_cast<HeadsUpDisplayLayerImpl*>(layerImpl);
    hudLayerImpl->setFontAtlas(m_fontAtlas.Pass());
}

}
