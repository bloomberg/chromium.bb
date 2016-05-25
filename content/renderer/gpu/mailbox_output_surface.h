// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_GPU_MAILBOX_OUTPUT_SURFACE_H_
#define CONTENT_RENDERER_GPU_MAILBOX_OUTPUT_SURFACE_H_

#include <stddef.h>
#include <stdint.h>

#include <queue>

#include "base/memory/ref_counted.h"
#include "cc/output/output_surface.h"
#include "cc/resources/resource_format.h"
#include "cc/resources/transferable_resource.h"
#include "ui/gfx/geometry/size.h"

namespace cc {
class CompositorFrameAck;
class ContextProvider;
class GLFrameData;
}

namespace content {
class FrameSwapMessageQueue;

// Implementation of CompositorOutputSurface that renders to textures which
// are sent to the browser through the mailbox extension.
// This class can be created only on the main thread, but then becomes pinned
// to a fixed thread when bindToClient is called.
class MailboxOutputSurface : public cc::OutputSurface {
 public:
  MailboxOutputSurface(
      uint32_t output_surface_id,
      scoped_refptr<cc::ContextProvider> context_provider,
      scoped_refptr<cc::ContextProvider> worker_context_provider);
  ~MailboxOutputSurface() override;

  // cc::OutputSurface implementation.
  bool BindToClient(cc::OutputSurfaceClient* client) override;
  void DetachFromClient() override;
  void EnsureBackbuffer() override;
  void DiscardBackbuffer() override;
  void Reshape(const gfx::Size& size, float scale_factor, bool alpha) override;
  void BindFramebuffer() override;
  void SwapBuffers(cc::CompositorFrame* frame) override;

 private:
  void ShortcutSwapAck(uint32_t output_surface_id,
                       std::unique_ptr<cc::GLFrameData> gl_frame_data);
  void OnSwapAck(uint32_t output_surface_id, const cc::CompositorFrameAck& ack);

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

  const uint32_t output_surface_id_;

  TransferableFrame current_backing_;
  std::deque<TransferableFrame> pending_textures_;
  std::queue<TransferableFrame> returned_textures_;

  uint32_t fbo_;
  bool is_backbuffer_discarded_;

  std::unique_ptr<cc::CompositorFrameAck> previous_frame_ack_;
  base::WeakPtrFactory<MailboxOutputSurface> weak_ptrs_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_GPU_MAILBOX_OUTPUT_SURFACE_H_
