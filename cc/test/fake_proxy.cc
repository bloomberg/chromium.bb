// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_proxy.h"

namespace cc {

void FakeProxy::SetLayerTreeHost(LayerTreeHost* host) {
  layer_tree_host_ = host;
}

bool FakeProxy::CompositeAndReadback(void* pixels, gfx::Rect rect) {
  return true;
}

bool FakeProxy::IsStarted() const { return true; }

void FakeProxy::CreateAndInitializeOutputSurface() {
  DCHECK(layer_tree_host_);
  layer_tree_host_->OnCreateAndInitializeOutputSurfaceAttempted(true);
}

const RendererCapabilities& FakeProxy::GetRendererCapabilities() const {
  return capabilities_;
}

RendererCapabilities& FakeProxy::GetRendererCapabilities() {
  return capabilities_;
}

bool FakeProxy::CommitRequested() const { return false; }

size_t FakeProxy::MaxPartialTextureUpdates() const {
  return max_partial_texture_updates_;
}

void FakeProxy::SetMaxPartialTextureUpdates(size_t max) {
  max_partial_texture_updates_ = max;
}

bool FakeProxy::CommitPendingForTesting() { return false; }

skia::RefPtr<SkPicture> FakeProxy::CapturePicture() {
  return skia::RefPtr<SkPicture>();
}

scoped_ptr<base::Value> FakeProxy::AsValue() const {
  scoped_ptr<base::DictionaryValue> state(new base::DictionaryValue());
  return state.PassAs<base::Value>();
}

}  // namespace cc
