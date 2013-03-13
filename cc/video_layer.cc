// Copyright 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/video_layer.h"

#include "cc/video_layer_impl.h"

namespace cc {

scoped_refptr<VideoLayer> VideoLayer::Create(VideoFrameProvider* provider) {
  return make_scoped_refptr(new VideoLayer(provider));
}

VideoLayer::VideoLayer(VideoFrameProvider* provider) : provider_(provider) {
  DCHECK(provider_);
}

VideoLayer::~VideoLayer() {}

scoped_ptr<LayerImpl> VideoLayer::CreateLayerImpl(LayerTreeImpl* tree_impl) {
  return VideoLayerImpl::Create(tree_impl, id(), provider_).PassAs<LayerImpl>();
}

}  // namespace cc
