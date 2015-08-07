// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_IN_PROCESS_SYNCHRONOUS_COMPOSITOR_CONTEXT_PROVIDER_H_
#define CONTENT_BROWSER_ANDROID_IN_PROCESS_SYNCHRONOUS_COMPOSITOR_CONTEXT_PROVIDER_H_

#include "content/common/gpu/client/context_provider_command_buffer.h"

namespace content {

// This class exists purely to ignore memory signals from the command buffer.
// Synchronous compositor manages memory by itself.
class SynchronousCompositorContextProvider
    : public ContextProviderCommandBuffer {
 public:
  SynchronousCompositorContextProvider(
      scoped_ptr<WebGraphicsContext3DCommandBufferImpl> context3d,
      CommandBufferContextType type);

  void SetMemoryPolicyChangedCallback(
      const MemoryPolicyChangedCallback& memory_policy_changed_callback)
      override;

 private:
  ~SynchronousCompositorContextProvider() override;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_IN_PROCESS_SYNCHRONOUS_COMPOSITOR_CONTEXT_PROVIDER_H_
