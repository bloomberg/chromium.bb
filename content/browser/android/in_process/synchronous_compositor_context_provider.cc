// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/in_process/synchronous_compositor_context_provider.h"

namespace content {

SynchronousCompositorContextProvider::SynchronousCompositorContextProvider(
    scoped_ptr<WebGraphicsContext3DCommandBufferImpl> context3d,
    CommandBufferContextType type)
    : ContextProviderCommandBuffer(context3d.Pass(), type) {}

SynchronousCompositorContextProvider::~SynchronousCompositorContextProvider() {}

void SynchronousCompositorContextProvider::SetMemoryPolicyChangedCallback(
    const MemoryPolicyChangedCallback& memory_policy_changed_callback) {
  // Intentional no-op.
}

}  // namespace content
