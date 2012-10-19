// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "cc/heads_up_display_layer.h"

#include "CCHeadsUpDisplayLayerImpl.h"
#include "CCLayerTreeHost.h"
#include "base/debug/trace_event.h"

namespace cc {

scoped_refptr<HeadsUpDisplayLayerChromium> HeadsUpDisplayLayerChromium::create()
{
    return make_scoped_refptr(new HeadsUpDisplayLayerChromium());
}

HeadsUpDisplayLayerChromium::HeadsUpDisplayLayerChromium()
    : LayerChromium()
{

    setBounds(IntSize(512, 128));
}

HeadsUpDisplayLayerChromium::~HeadsUpDisplayLayerChromium()
{
}

void HeadsUpDisplayLayerChromium::update(CCTextureUpdateQueue&, const CCOcclusionTracker*, CCRenderingStats&)
{
    const CCLayerTreeSettings& settings = layerTreeHost()->settings();
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

bool HeadsUpDisplayLayerChromium::drawsContent() const
{
    return true;
}

void HeadsUpDisplayLayerChromium::setFontAtlas(scoped_ptr<CCFontAtlas> fontAtlas)
{
    m_fontAtlas = fontAtlas.Pass();
    setNeedsCommit();
}

scoped_ptr<CCLayerImpl> HeadsUpDisplayLayerChromium::createCCLayerImpl()
{
    return CCHeadsUpDisplayLayerImpl::create(m_layerId).PassAs<CCLayerImpl>();
}

void HeadsUpDisplayLayerChromium::pushPropertiesTo(CCLayerImpl* layerImpl)
{
    LayerChromium::pushPropertiesTo(layerImpl);

    if (!m_fontAtlas.get())
        return;

    CCHeadsUpDisplayLayerImpl* hudLayerImpl = static_cast<CCHeadsUpDisplayLayerImpl*>(layerImpl);
    hudLayerImpl->setFontAtlas(m_fontAtlas.Pass());
}

}
