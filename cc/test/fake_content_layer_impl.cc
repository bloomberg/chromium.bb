// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_content_layer_impl.h"

namespace cc {

FakeContentLayerImpl::FakeContentLayerImpl(LayerTreeImpl* tree_impl, int id)
    : TiledLayerImpl(tree_impl, id),
      lost_output_surface_count_(0) {
}

FakeContentLayerImpl::~FakeContentLayerImpl() {}

bool FakeContentLayerImpl::HaveResourceForTileAt(int i, int j) {
  return hasResourceIdForTileAt(i, j);
}

void FakeContentLayerImpl::didLoseOutputSurface() {
  ++lost_output_surface_count_;
}

}  // namespace cc
