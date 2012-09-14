// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#if USE(ACCELERATED_COMPOSITING)

#include "IOSurfaceLayerChromium.h"

#include "CCIOSurfaceLayerImpl.h"

namespace cc {

PassRefPtr<IOSurfaceLayerChromium> IOSurfaceLayerChromium::create()
{
    return adoptRef(new IOSurfaceLayerChromium());
}

IOSurfaceLayerChromium::IOSurfaceLayerChromium()
    : LayerChromium()
    , m_ioSurfaceId(0)
{
}

IOSurfaceLayerChromium::~IOSurfaceLayerChromium()
{
}

void IOSurfaceLayerChromium::setIOSurfaceProperties(uint32_t ioSurfaceId, const IntSize& size)
{
    m_ioSurfaceId = ioSurfaceId;
    m_ioSurfaceSize = size;
    setNeedsCommit();
}

PassOwnPtr<CCLayerImpl> IOSurfaceLayerChromium::createCCLayerImpl()
{
    return CCIOSurfaceLayerImpl::create(m_layerId);
}

bool IOSurfaceLayerChromium::drawsContent() const
{
    return m_ioSurfaceId && LayerChromium::drawsContent();
}

void IOSurfaceLayerChromium::pushPropertiesTo(CCLayerImpl* layer)
{
    LayerChromium::pushPropertiesTo(layer);

    CCIOSurfaceLayerImpl* textureLayer = static_cast<CCIOSurfaceLayerImpl*>(layer);
    textureLayer->setIOSurfaceProperties(m_ioSurfaceId, m_ioSurfaceSize);
}

}
#endif // USE(ACCELERATED_COMPOSITING)
