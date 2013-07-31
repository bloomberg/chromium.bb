// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/gpu/mailbox_output_surface.h"

#include "base/logging.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/compositor_frame_ack.h"
#include "cc/output/gl_frame_data.h"
#include "third_party/WebKit/public/platform/WebGraphicsContext3D.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"

using cc::CompositorFrame;
using cc::GLFrameData;
using gpu::Mailbox;

namespace content {

MailboxOutputSurface::MailboxOutputSurface(
    int32 routing_id,
    uint32 output_surface_id,
    WebGraphicsContext3DCommandBufferImpl* context3D,
    cc::SoftwareOutputDevice* software_device)
    : CompositorOutputSurface(routing_id,
                              output_surface_id,
                              context3D,
                              software_device,
                              true),
      fbo_(0),
      is_backbuffer_discarded_(false) {
  pending_textures_.push_back(TransferableFrame());
  capabilities_.max_frames_pending = 1;
}

MailboxOutputSurface::~MailboxOutputSurface() {
  DiscardBackbuffer();
  while (!pending_textures_.empty()) {
    if (pending_textures_.front().texture_id)
      context3d_->deleteTexture(pending_textures_.front().texture_id);
    pending_textures_.pop_front();
  }
}

void MailboxOutputSurface::EnsureBackbuffer() {
  is_backbuffer_discarded_ = false;

  if (!current_backing_.texture_id) {
    // Find a texture of matching size to recycle.
    while (!returned_textures_.empty()) {
      TransferableFrame& texture = returned_textures_.front();
      if (texture.size == surface_size_) {
        current_backing_ = texture;
        if (current_backing_.sync_point)
          context3d_->waitSyncPoint(current_backing_.sync_point);
        returned_textures_.pop();
        break;
      }

      context3d_->deleteTexture(texture.texture_id);
      returned_textures_.pop();
    }

    if (!current_backing_.texture_id) {
      current_backing_.texture_id = context3d_->createTexture();
      current_backing_.size = surface_size_;
      context3d_->bindTexture(GL_TEXTURE_2D, current_backing_.texture_id);
      context3d_->texParameteri(
          GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      context3d_->texParameteri(
          GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      context3d_->texParameteri(
          GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      context3d_->texParameteri(
          GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      context3d_->texImage2D(
          GL_TEXTURE_2D, 0, GL_RGBA,
          surface_size_.width(), surface_size_.height(), 0,
          GL_RGBA, GL_UNSIGNED_BYTE, NULL);
      context3d_->genMailboxCHROMIUM(current_backing_.mailbox.name);
      context3d_->produceTextureCHROMIUM(
          GL_TEXTURE_2D, current_backing_.mailbox.name);
    }
  }
}

void MailboxOutputSurface::DiscardBackbuffer() {
  is_backbuffer_discarded_ = true;

  if (current_backing_.texture_id) {
    context3d_->deleteTexture(current_backing_.texture_id);
    current_backing_ = TransferableFrame();
  }

  while (!returned_textures_.empty()) {
    const TransferableFrame& frame = returned_textures_.front();
    context3d_->deleteTexture(frame.texture_id);
    returned_textures_.pop();
  }

  if (fbo_) {
    context3d_->bindFramebuffer(GL_FRAMEBUFFER, fbo_);
    context3d_->deleteFramebuffer(fbo_);
    fbo_ = 0;
  }
}

void MailboxOutputSurface::Reshape(gfx::Size size, float scale_factor) {
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

  if (!fbo_)
    fbo_ = context3d_->createFramebuffer();
  context3d_->bindFramebuffer(GL_FRAMEBUFFER, fbo_);
  context3d_->framebufferTexture2D(
      GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
      current_backing_.texture_id, 0);
}

void MailboxOutputSurface::OnSwapAck(uint32 output_surface_id,
                                     const cc::CompositorFrameAck& ack) {
  // Ignore message if it's a stale one coming from a different output surface
  // (e.g. after a lost context).
  if (output_surface_id != output_surface_id_) {
    CompositorOutputSurface::OnSwapAck(output_surface_id, ack);
    return;
  }
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
    it->sync_point = ack.gl_frame_data->sync_point;

    if (!is_backbuffer_discarded_) {
      returned_textures_.push(*it);
    } else {
      context3d_->deleteTexture(it->texture_id);
    }

    pending_textures_.erase(it);
  } else {
    DCHECK(!pending_textures_.empty());
    // The browser always keeps one texture as the frontbuffer.
    // If it does not return a mailbox, it discarded the frontbuffer which is
    // the oldest texture we sent.
    uint32 texture_id = pending_textures_.front().texture_id;
    if (texture_id)
      context3d_->deleteTexture(texture_id);
    pending_textures_.pop_front();
  }
  CompositorOutputSurface::OnSwapAck(output_surface_id, ack);
}

void MailboxOutputSurface::SwapBuffers(cc::CompositorFrame* frame) {
  DCHECK(frame->gl_frame_data);
  DCHECK(!surface_size_.IsEmpty());
  DCHECK(surface_size_ == current_backing_.size);
  DCHECK(frame->gl_frame_data->size == current_backing_.size);
  DCHECK(!current_backing_.mailbox.IsZero() || context3d_->isContextLost());

  frame->gl_frame_data->mailbox = current_backing_.mailbox;
  context3d_->flush();
  frame->gl_frame_data->sync_point = context3d_->insertSyncPoint();
  CompositorOutputSurface::SwapBuffers(frame);

  pending_textures_.push_back(current_backing_);
  current_backing_ = TransferableFrame();
}

size_t MailboxOutputSurface::GetNumAcksPending() {
  DCHECK(pending_textures_.size());
  return pending_textures_.size() - 1;
}

}  // namespace content
