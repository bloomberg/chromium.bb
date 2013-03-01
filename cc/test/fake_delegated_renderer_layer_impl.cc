// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_delegated_renderer_layer_impl.h"

#include "cc/delegated_frame_data.h"

namespace cc {

FakeDelegatedRendererLayerImpl::FakeDelegatedRendererLayerImpl(
    LayerTreeImpl* tree_impl, int id)
    : DelegatedRendererLayerImpl(tree_impl, id) {}

FakeDelegatedRendererLayerImpl::~FakeDelegatedRendererLayerImpl() {}

}  // namespace cc
