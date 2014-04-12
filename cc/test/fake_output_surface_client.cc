// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_output_surface_client.h"

namespace cc {

bool FakeOutputSurfaceClient::DeferredInitialize(
    scoped_refptr<ContextProvider> offscreen_context_provider) {
  deferred_initialize_called_ = true;
  return deferred_initialize_result_;
}

void FakeOutputSurfaceClient::BeginFrame(const BeginFrameArgs& args) {
  begin_frame_count_++;
}

void FakeOutputSurfaceClient::DidLoseOutputSurface() {
  did_lose_output_surface_called_ = true;
}

void FakeOutputSurfaceClient::SetMemoryPolicy(
    const ManagedMemoryPolicy& policy) {
  memory_policy_ = policy;
}

}  // namespace cc
