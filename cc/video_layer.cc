// Copyright 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "cc/video_layer.h"

#include "CCVideoLayerImpl.h"

namespace cc {

scoped_refptr<VideoLayer> VideoLayer::create(WebKit::WebVideoFrameProvider* provider)
{
    return make_scoped_refptr(new VideoLayer(provider));
}

VideoLayer::VideoLayer(WebKit::WebVideoFrameProvider* provider)
    : Layer()
    , m_provider(provider)
{
    DCHECK(m_provider);
}

VideoLayer::~VideoLayer()
{
}

scoped_ptr<LayerImpl> VideoLayer::createLayerImpl()
{
    return VideoLayerImpl::create(m_layerId, m_provider).PassAs<LayerImpl>();
}

} // namespace cc
