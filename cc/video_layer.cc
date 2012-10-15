// Copyright 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "VideoLayerChromium.h"

#include "CCVideoLayerImpl.h"

namespace cc {

scoped_refptr<VideoLayerChromium> VideoLayerChromium::create(WebKit::WebVideoFrameProvider* provider)
{
    return make_scoped_refptr(new VideoLayerChromium(provider));
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

scoped_ptr<CCLayerImpl> VideoLayerChromium::createCCLayerImpl()
{
    return CCVideoLayerImpl::create(m_layerId, m_provider).PassAs<CCLayerImpl>();
}

} // namespace cc
