// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_GPU_MAILBOX_OUTPUT_SURFACE_H_
#define CONTENT_RENDERER_GPU_MAILBOX_OUTPUT_SURFACE_H_

#include <queue>

#include "cc/transferable_resource.h"
#include "content/renderer/gpu/compositor_output_surface.h"
#include "ui/gfx/size.h"

namespace cc {
class CompositorFrameAck;
}

namespace content {

// Implementation of CompositorOutputSurface that renders to textures which
// are sent to the browser through the mailbox extension.
// This class can be created only on the main thread, but then becomes pinned
// to a fixed thread when bindToClient is called.
class MailboxOutputSurface : public CompositorOutputSurface {
 public:
  MailboxOutputSurface(int32 routing_id,
                       WebKit::WebGraphicsContext3D* context3d,
                       cc::SoftwareOutputDevice* software);
  virtual ~MailboxOutputSurface();

  // cc::OutputSurface implementation.
  virtual void SendFrameToParentCompositor(cc::CompositorFrame* frame) OVERRIDE;
  virtual void EnsureBackbuffer() OVERRIDE;
  virtual void DiscardBackbuffer() OVERRIDE;
  virtual void Reshape(gfx::Size size) OVERRIDE;
  virtual void BindFramebuffer() OVERRIDE;
  virtual void PostSubBuffer(gfx::Rect rect) OVERRIDE;
  virtual void SwapBuffers() OVERRIDE;

 private:
  // CompositorOutputSurface overrides.
  virtual void OnSwapAck(const cc::CompositorFrameAck& ack) OVERRIDE;

  struct TransferableFrame
  {
    TransferableFrame()
        : texture_id(0) {}

    TransferableFrame(uint32 texture_id,
                      const gpu::Mailbox& mailbox,
                      const gfx::Size size)
        : texture_id(texture_id),
          mailbox(mailbox),
          size(size) {}

    uint32 texture_id;
    gpu::Mailbox mailbox;
    gfx::Size size;
  };
  TransferableFrame current_backing_;
  std::queue<TransferableFrame> returned_textures_;

  gfx::Size size_;
  uint32 fbo_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_GPU_MAILBOX_OUTPUT_SURFACE_H_
