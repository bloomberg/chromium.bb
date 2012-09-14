// Copyright 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#if USE(ACCELERATED_COMPOSITING)
#include "VideoLayerChromium.h"

#include "CCVideoLayerImpl.h"

namespace cc {

PassRefPtr<VideoLayerChromium> VideoLayerChromium::create(WebKit::WebVideoFrameProvider* provider)
{
    return adoptRef(new VideoLayerChromium(provider));
}

VideoLayerChromium::VideoLayerChromium(WebKit::WebVideoFrameProvider* provider)
    : LayerChromium()
    , m_provider(provider)
{
    ASSERT(m_provider);
}

VideoLayerChromium::~VideoLayerChromium()
{
}

PassOwnPtr<CCLayerImpl> VideoLayerChromium::createCCLayerImpl()
{
    return CCVideoLayerImpl::create(m_layerId, m_provider);
}

} // namespace cc

#endif // USE(ACCELERATED_COMPOSITING)
