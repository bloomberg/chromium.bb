// Copyright 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/video_layer.h"

#include "cc/video_layer_impl.h"

namespace cc {

scoped_refptr<VideoLayer> VideoLayer::create(
    WebKit::WebVideoFrameProvider* provider,
    const FrameUnwrapper& unwrapper)
{
    return make_scoped_refptr(new VideoLayer(provider, unwrapper));
}

VideoLayer::VideoLayer(WebKit::WebVideoFrameProvider* provider,
                       const FrameUnwrapper& unwrapper)
    : m_provider(provider)
    , m_unwrapper(unwrapper)
{
    DCHECK(m_provider);
}

VideoLayer::~VideoLayer()
{
}

scoped_ptr<LayerImpl> VideoLayer::createLayerImpl(LayerTreeHostImpl* hostImpl)
{
    return VideoLayerImpl::create(hostImpl, m_layerId, m_provider, m_unwrapper).PassAs<LayerImpl>();
}

}  // namespace cc
