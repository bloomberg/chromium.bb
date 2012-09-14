// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "HeadsUpDisplayLayerChromium.h"

#include "CCHeadsUpDisplayLayerImpl.h"
#include "CCLayerTreeHost.h"
#include "TraceEvent.h"

namespace cc {

PassRefPtr<HeadsUpDisplayLayerChromium> HeadsUpDisplayLayerChromium::create()
{
    return adoptRef(new HeadsUpDisplayLayerChromium());
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

void HeadsUpDisplayLayerChromium::setFontAtlas(PassOwnPtr<CCFontAtlas> fontAtlas)
{
    m_fontAtlas = fontAtlas;
    setNeedsCommit();
}

PassOwnPtr<CCLayerImpl> HeadsUpDisplayLayerChromium::createCCLayerImpl()
{
    return CCHeadsUpDisplayLayerImpl::create(m_layerId);
}

void HeadsUpDisplayLayerChromium::pushPropertiesTo(CCLayerImpl* layerImpl)
{
    LayerChromium::pushPropertiesTo(layerImpl);

    if (!m_fontAtlas)
        return;

    CCHeadsUpDisplayLayerImpl* hudLayerImpl = static_cast<CCHeadsUpDisplayLayerImpl*>(layerImpl);
    hudLayerImpl->setFontAtlas(m_fontAtlas.release());
}

}
