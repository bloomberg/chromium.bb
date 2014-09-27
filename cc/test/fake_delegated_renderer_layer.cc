// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_delegated_renderer_layer.h"

#include "cc/test/fake_delegated_renderer_layer_impl.h"

namespace cc {

FakeDelegatedRendererLayer::FakeDelegatedRendererLayer(
    DelegatedFrameProvider* frame_provider)
    : DelegatedRendererLayer(frame_provider) {}

FakeDelegatedRendererLayer::~FakeDelegatedRendererLayer() {}

scoped_ptr<LayerImpl> FakeDelegatedRendererLayer::CreateLayerImpl(
    LayerTreeImpl* tree_impl) {
  return FakeDelegatedRendererLayerImpl::Create(
      tree_impl, layer_id_).PassAs<LayerImpl>();
}

}  // namespace cc
