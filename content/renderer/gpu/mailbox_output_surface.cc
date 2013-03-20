// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/gpu/mailbox_output_surface.h"

#include "base/logging.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/compositor_frame_ack.h"
#include "cc/output/gl_frame_data.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebGraphicsContext3D.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"

using cc::CompositorFrame;
using cc::GLFrameData;
using gpu::Mailbox;
using WebKit::WebGraphicsContext3D;

namespace content {

MailboxOutputSurface::MailboxOutputSurface(
    int32 routing_id,
    WebGraphicsContext3D* context3D,
    cc::SoftwareOutputDevice* software_device)
    : CompositorOutputSurface(routing_id,
                              context3D,
                              software_device),
  fbo_(0) {
}

MailboxOutputSurface::~MailboxOutputSurface() {
  DiscardBackbuffer();
}

void MailboxOutputSurface::EnsureBackbuffer() {
  if (!current_backing_.texture_id) {
    // Find a texture of matching size to recycle.
    while (!returned_textures_.empty()) {
      TransferableFrame& texture = returned_textures_.front();
      if (texture.size == size_) {
        current_backing_ = texture;
        returned_textures_.pop();
        break;
      }

      context3d_->deleteTexture(texture.texture_id);
      returned_textures_.pop();
    }

    if (!current_backing_.texture_id) {
      current_backing_.texture_id = context3d_->createTexture();
      current_backing_.size = size_;
      context3d_->genMailboxCHROMIUM(current_backing_.mailbox.name);
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
          GL_TEXTURE_2D, 0, GL_RGBA, size_.width(), size_.height(), 0,
          GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    }
  }
}

void MailboxOutputSurface::DiscardBackbuffer() {
  if (current_backing_.texture_id) {
    context3d_->deleteTexture(current_backing_.texture_id);
    current_backing_ = TransferableFrame();
  }

  while (!returned_textures_.empty()) {
    context3d_->deleteTexture(returned_textures_.front().texture_id);
    returned_textures_.pop();
  }

  if (fbo_) {
    context3d_->bindFramebuffer(GL_FRAMEBUFFER, fbo_);
    context3d_->deleteFramebuffer(fbo_);
    fbo_ = 0;
  }
}

void MailboxOutputSurface::Reshape(gfx::Size size) {
  if (size == size_)
    return;

  size_ = size;
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

void MailboxOutputSurface::SendFrameToParentCompositor(
    cc::CompositorFrame* frame) {
  frame->gl_frame_data.reset(new GLFrameData());

  DCHECK(!size_.IsEmpty());
  DCHECK(size_ == current_backing_.size);
  DCHECK(!current_backing_.mailbox.IsZero());

  context3d_->framebufferTexture2D(
      GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
// TODO(sievers): Remove ifdef (crbug.com/222018)
#ifndef NDEBUG
  context3d_->bindFramebuffer(GL_FRAMEBUFFER, 0);
#endif
  context3d_->bindTexture(GL_TEXTURE_2D, current_backing_.texture_id);
  context3d_->produceTextureCHROMIUM(
      GL_TEXTURE_2D, current_backing_.mailbox.name);
  frame->gl_frame_data->mailbox = current_backing_.mailbox;
  frame->gl_frame_data->size = current_backing_.size;
  context3d_->flush();
  frame->gl_frame_data->sync_point = context3d_->insertSyncPoint();
  CompositorOutputSurface::SendFrameToParentCompositor(frame);

  // TODO(sievers): Reuse the texture.
  context3d_->deleteTexture(current_backing_.texture_id);
  current_backing_ = TransferableFrame();
}

void MailboxOutputSurface::OnSwapAck(const cc::CompositorFrameAck& ack) {
  if (!ack.gl_frame_data->mailbox.IsZero()) {
    DCHECK(!ack.gl_frame_data->size.IsEmpty());
    uint32 texture_id = context3d_->createTexture();
    TransferableFrame texture(
        texture_id, ack.gl_frame_data->mailbox, ack.gl_frame_data->size);

    context3d_->bindTexture(GL_TEXTURE_2D, texture_id);

    // If the consumer is bouncing back the same texture (i.e. skipping the
    // frame), we don't have to synchronize here (sync_point == 0).
    // TODO: Consider delaying the wait and consume until BindFramebuffer.
    if (ack.gl_frame_data->sync_point)
      context3d_->waitSyncPoint(ack.gl_frame_data->sync_point);

    context3d_->consumeTextureCHROMIUM(
        GL_TEXTURE_2D, ack.gl_frame_data->mailbox.name);
    returned_textures_.push(texture);
  }
  CompositorOutputSurface::OnSwapAck(ack);
}

void MailboxOutputSurface::SwapBuffers() {
}

void MailboxOutputSurface::PostSubBuffer(gfx::Rect rect) {
  NOTIMPLEMENTED()
      << "Partial swap not supported with composite-to-mailbox yet.";
}

}  // namespace content
