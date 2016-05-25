// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_GPU_MAILBOX_OUTPUT_SURFACE_H_
#define CONTENT_RENDERER_GPU_MAILBOX_OUTPUT_SURFACE_H_

#include <stddef.h>
#include <stdint.h>

#include <queue>

#include "base/memory/ref_counted.h"
#include "cc/resources/resource_format.h"
#include "cc/resources/transferable_resource.h"
#include "content/renderer/gpu/compositor_output_surface.h"
#include "ui/gfx/geometry/size.h"

namespace cc {
class CompositorFrameAck;
}

namespace content {
class FrameSwapMessageQueue;

// Implementation of CompositorOutputSurface that renders to textures which
// are sent to the browser through the mailbox extension.
// This class can be created only on the main thread, but then becomes pinned
// to a fixed thread when bindToClient is called.
class MailboxOutputSurface : public CompositorOutputSurface {
 public:
  MailboxOutputSurface(
      int32_t routing_id,
      uint32_t output_surface_id,
      const scoped_refptr<ContextProviderCommandBuffer>& context_provider,
      const scoped_refptr<ContextProviderCommandBuffer>&
          worker_context_provider,
      scoped_refptr<FrameSwapMessageQueue> swap_frame_message_queue,
      cc::ResourceFormat format);
  ~MailboxOutputSurface() override;

  // cc::OutputSurface implementation.
  void DetachFromClient() override;
  void EnsureBackbuffer() override;
  void DiscardBackbuffer() override;
  void Reshape(const gfx::Size& size, float scale_factor, bool alpha) override;
  void BindFramebuffer() override;
  void SwapBuffers(cc::CompositorFrame* frame) override;

 private:
  // CompositorOutputSurface overrides.
  void OnSwapAck(uint32_t output_surface_id,
                 const cc::CompositorFrameAck& ack) override;

  size_t GetNumAcksPending();

  struct TransferableFrame {
    TransferableFrame();
    TransferableFrame(uint32_t texture_id,
                      const gpu::Mailbox& mailbox,
                      const gfx::Size size);

    uint32_t texture_id;
    gpu::Mailbox mailbox;
    gpu::SyncToken sync_token;
    gfx::Size size;
  };

  TransferableFrame current_backing_;
  std::deque<TransferableFrame> pending_textures_;
  std::queue<TransferableFrame> returned_textures_;

  uint32_t fbo_;
  bool is_backbuffer_discarded_;
  cc::ResourceFormat format_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_GPU_MAILBOX_OUTPUT_SURFACE_H_
