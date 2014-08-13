// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_proxy.h"

namespace cc {

void FakeProxy::SetLayerTreeHost(LayerTreeHost* host) {
  layer_tree_host_ = host;
}

bool FakeProxy::IsStarted() const { return true; }

const RendererCapabilities& FakeProxy::GetRendererCapabilities() const {
  return capabilities_;
}

RendererCapabilities& FakeProxy::GetRendererCapabilities() {
  return capabilities_;
}

bool FakeProxy::BeginMainFrameRequested() const { return false; }

bool FakeProxy::CommitRequested() const { return false; }

size_t FakeProxy::MaxPartialTextureUpdates() const {
  return max_partial_texture_updates_;
}

void FakeProxy::SetMaxPartialTextureUpdates(size_t max) {
  max_partial_texture_updates_ = max;
}

bool FakeProxy::SupportsImplScrolling() const { return false; }

bool FakeProxy::MainFrameWillHappenForTesting() {
  return false;
}

void FakeProxy::AsValueInto(base::debug::TracedValue*) const {
}

}  // namespace cc
