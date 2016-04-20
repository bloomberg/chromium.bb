// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_GPU_DELEGATED_COMPOSITOR_OUTPUT_SURFACE_H_
#define CONTENT_RENDERER_GPU_DELEGATED_COMPOSITOR_OUTPUT_SURFACE_H_

#include <stdint.h>

#include "base/memory/ref_counted.h"
#include "content/renderer/gpu/compositor_output_surface.h"

namespace content {
class FrameSwapMessageQueue;

class DelegatedCompositorOutputSurface : public CompositorOutputSurface {
 public:
  DelegatedCompositorOutputSurface(
      int32_t routing_id,
      uint32_t output_surface_id,
      const scoped_refptr<ContextProviderCommandBuffer>& context_provider,
      const scoped_refptr<ContextProviderCommandBuffer>&
          worker_context_provider,
      const scoped_refptr<cc::VulkanContextProvider>& vulkan_context_provider,
      scoped_refptr<FrameSwapMessageQueue> swap_frame_message_queue);
  ~DelegatedCompositorOutputSurface() override {}
};

}  // namespace content

#endif  // CONTENT_RENDERER_GPU_DELEGATED_COMPOSITOR_OUTPUT_SURFACE_H_
