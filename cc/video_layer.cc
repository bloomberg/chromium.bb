// Copyright 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/video_layer.h"

#include "cc/video_layer_impl.h"

namespace cc {

scoped_refptr<VideoLayer> VideoLayer::create(VideoFrameProvider* provider)
{
    return make_scoped_refptr(new VideoLayer(provider));
}

VideoLayer::VideoLayer(VideoFrameProvider* provider)
    : m_provider(provider)
{
    DCHECK(m_provider);
}

VideoLayer::~VideoLayer()
{
}

scoped_ptr<LayerImpl> VideoLayer::createLayerImpl(LayerTreeImpl* treeImpl)
{
    return VideoLayerImpl::create(treeImpl, m_layerId, m_provider).PassAs<LayerImpl>();
}

}  // namespace cc
