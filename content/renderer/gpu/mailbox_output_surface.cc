// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/gpu/mailbox_output_surface.h"

#include "base/logging.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/compositor_frame_ack.h"
#include "cc/output/gl_frame_data.h"
#include "cc/output/managed_memory_policy.h"
#include "cc/output/output_surface_client.h"
#include "cc/resources/resource_provider.h"
#include "content/renderer/gpu/frame_swap_message_queue.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/common/gpu_memory_allocation.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"

using cc::CompositorFrame;
using cc::GLFrameData;
using cc::ResourceProvider;
using gpu::Mailbox;
using gpu::gles2::GLES2Interface;

namespace content {

MailboxOutputSurface::MailboxOutputSurface(
    uint32_t output_surface_id,
    scoped_refptr<cc::ContextProvider> context_provider,
    scoped_refptr<cc::ContextProvider> worker_context_provider)
    : cc::OutputSurface(std::move(context_provider),
                        std::move(worker_context_provider),
                        nullptr),
      output_surface_id_(output_surface_id),
      fbo_(0),
      is_backbuffer_discarded_(false),
      weak_ptrs_(this) {
  pending_textures_.push_back(TransferableFrame());
  capabilities_.uses_default_gl_framebuffer = false;
}

MailboxOutputSurface::~MailboxOutputSurface() = default;

bool MailboxOutputSurface::BindToClient(cc::OutputSurfaceClient* client) {
  if (!cc::OutputSurface::BindToClient(client))
    return false;

  if (!context_provider()) {
    // Without a GPU context, the memory policy otherwise wouldn't be set.
    client->SetMemoryPolicy(cc::ManagedMemoryPolicy(
        128 * 1024 * 1024, gpu::MemoryAllocation::CUTOFF_ALLOW_NICE_TO_HAVE,
        base::SharedMemory::GetHandleLimit() / 3));
  }

  return true;
}

void MailboxOutputSurface::DetachFromClient() {
  DiscardBackbuffer();
  while (!pending_textures_.empty()) {
    if (pending_textures_.front().texture_id) {
      context_provider_->ContextGL()->DeleteTextures(
          1, &pending_textures_.front().texture_id);
    }
    pending_textures_.pop_front();
  }
  cc::OutputSurface::DetachFromClient();
}

void MailboxOutputSurface::EnsureBackbuffer() {
  is_backbuffer_discarded_ = false;

  GLES2Interface* gl = context_provider_->ContextGL();

  if (!current_backing_.texture_id) {
    // Find a texture of matching size to recycle.
    while (!returned_textures_.empty()) {
      TransferableFrame& texture = returned_textures_.front();
      if (texture.size == surface_size_) {
        current_backing_ = texture;
        if (current_backing_.sync_token.HasData())
          gl->WaitSyncTokenCHROMIUM(current_backing_.sync_token.GetConstData());
        returned_textures_.pop();
        break;
      }

      gl->DeleteTextures(1, &texture.texture_id);
      returned_textures_.pop();
    }

    if (!current_backing_.texture_id) {
      gl->GenTextures(1, &current_backing_.texture_id);
      current_backing_.size = surface_size_;
      gl->BindTexture(GL_TEXTURE_2D, current_backing_.texture_id);
      gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      gl->TexImage2D(GL_TEXTURE_2D, 0, GLInternalFormat(cc::RGBA_8888),
                     surface_size_.width(), surface_size_.height(), 0,
                     GLDataFormat(cc::RGBA_8888), GLDataType(cc::RGBA_8888),
                     NULL);
      gl->GenMailboxCHROMIUM(current_backing_.mailbox.name);
      gl->ProduceTextureCHROMIUM(GL_TEXTURE_2D, current_backing_.mailbox.name);
    }
  }
}

void MailboxOutputSurface::DiscardBackbuffer() {
  is_backbuffer_discarded_ = true;

  GLES2Interface* gl = context_provider_->ContextGL();

  if (current_backing_.texture_id) {
    gl->DeleteTextures(1, &current_backing_.texture_id);
    current_backing_ = TransferableFrame();
  }

  while (!returned_textures_.empty()) {
    const TransferableFrame& frame = returned_textures_.front();
    gl->DeleteTextures(1, &frame.texture_id);
    returned_textures_.pop();
  }

  if (fbo_) {
    gl->BindFramebuffer(GL_FRAMEBUFFER, fbo_);
    gl->DeleteFramebuffers(1, &fbo_);
    fbo_ = 0;
  }
}

void MailboxOutputSurface::Reshape(const gfx::Size& size,
                                   float scale_factor,
                                   bool alpha) {
  if (size == surface_size_)
    return;

  surface_size_ = size;
  device_scale_factor_ = scale_factor;
  DiscardBackbuffer();
  EnsureBackbuffer();
}

void MailboxOutputSurface::BindFramebuffer() {
  EnsureBackbuffer();
  DCHECK(current_backing_.texture_id);

  GLES2Interface* gl = context_provider_->ContextGL();

  if (!fbo_)
    gl->GenFramebuffers(1, &fbo_);
  gl->BindFramebuffer(GL_FRAMEBUFFER, fbo_);
  gl->FramebufferTexture2D(GL_FRAMEBUFFER,
                           GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D,
                           current_backing_.texture_id,
                           0);
}

void MailboxOutputSurface::OnSwapAck(uint32_t output_surface_id,
                                     const cc::CompositorFrameAck& ack) {
  // Ignore message if it's a stale one coming from a different output surface
  // (e.g. after a lost context).
  if (output_surface_id != output_surface_id_)
    return;

  if (!ack.gl_frame_data->mailbox.IsZero()) {
    DCHECK(!ack.gl_frame_data->size.IsEmpty());
    // The browser could be returning the oldest or any other pending texture
    // if it decided to skip a frame.
    std::deque<TransferableFrame>::iterator it;
    for (it = pending_textures_.begin(); it != pending_textures_.end(); it++) {
      DCHECK(!it->mailbox.IsZero());
      if (!memcmp(it->mailbox.name,
                  ack.gl_frame_data->mailbox.name,
                  sizeof(it->mailbox.name))) {
        DCHECK(it->size == ack.gl_frame_data->size);
        break;
      }
    }
    DCHECK(it != pending_textures_.end());
    it->sync_token = ack.gl_frame_data->sync_token;

    if (!is_backbuffer_discarded_) {
      returned_textures_.push(*it);
    } else {
      context_provider_->ContextGL()->DeleteTextures(1, &it->texture_id);
    }

    pending_textures_.erase(it);
  } else {
    DCHECK(!pending_textures_.empty());
    // The browser always keeps one texture as the frontbuffer.
    // If it does not return a mailbox, it discarded the frontbuffer which is
    // the oldest texture we sent.
    uint32_t texture_id = pending_textures_.front().texture_id;
    if (texture_id)
      context_provider_->ContextGL()->DeleteTextures(1, &texture_id);
    pending_textures_.pop_front();
  }

  ReclaimResources(&ack);
  client_->DidSwapBuffersComplete();
}

void MailboxOutputSurface::ShortcutSwapAck(
    uint32_t output_surface_id,
    std::unique_ptr<cc::GLFrameData> gl_frame_data) {
  if (!previous_frame_ack_) {
    previous_frame_ack_.reset(new cc::CompositorFrameAck);
    previous_frame_ack_->gl_frame_data.reset(new cc::GLFrameData);
  }

  OnSwapAck(output_surface_id, *previous_frame_ack_);

  previous_frame_ack_->gl_frame_data = std::move(gl_frame_data);
}

void MailboxOutputSurface::SwapBuffers(cc::CompositorFrame* frame) {
  // This class is here to support layout tests that are currently
  // doing a readback in the renderer instead of the browser. So they
  // are using deprecated code paths in the renderer and don't need to
  // actually swap anything to the browser. We shortcut the swap to the
  // browser here and just ack directly within the renderer process.
  // Once crbug.com/311404 is fixed, this can be removed.

  // This would indicate that crbug.com/311404 is being fixed, and this
  // block needs to be removed.
  DCHECK(!frame->delegated_frame_data);

  DCHECK(frame->gl_frame_data);
  DCHECK(!surface_size_.IsEmpty());
  DCHECK(surface_size_ == current_backing_.size);
  DCHECK(frame->gl_frame_data->size == current_backing_.size);
  DCHECK(!current_backing_.mailbox.IsZero() ||
         context_provider_->ContextGL()->GetGraphicsResetStatusKHR() !=
             GL_NO_ERROR);

  frame->gl_frame_data->mailbox = current_backing_.mailbox;

  gpu::gles2::GLES2Interface* gl = context_provider_->ContextGL();

  const GLuint64 fence_sync = gl->InsertFenceSyncCHROMIUM();
  gl->Flush();
  gl->GenSyncTokenCHROMIUM(fence_sync,
                           frame->gl_frame_data->sync_token.GetData());

  context_provider()->ContextSupport()->SignalSyncToken(
      frame->gl_frame_data->sync_token,
      base::Bind(&MailboxOutputSurface::ShortcutSwapAck,
                 weak_ptrs_.GetWeakPtr(), output_surface_id_,
                 base::Passed(&frame->gl_frame_data)));

  pending_textures_.push_back(current_backing_);
  current_backing_ = TransferableFrame();

  client_->DidSwapBuffers();
}

MailboxOutputSurface::TransferableFrame::TransferableFrame() : texture_id(0) {}

MailboxOutputSurface::TransferableFrame::TransferableFrame(
    uint32_t texture_id,
    const gpu::Mailbox& mailbox,
    const gfx::Size size)
    : texture_id(texture_id), mailbox(mailbox), size(size) {}

}  // namespace content
