// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/renderers/video_overlay_factory.h"

#include "base/bind.h"
#include "base/location.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "media/base/video_frame.h"
#include "media/renderers/gpu_video_accelerator_factories.h"

namespace media {

VideoOverlayFactory::VideoOverlayFactory(
    GpuVideoAcceleratorFactories* gpu_factories)
    : gpu_factories_(gpu_factories), texture_(0), image_id_(0) {
  if (gpu_factories_) {
    DCHECK(gpu_factories_->GetTaskRunner()->BelongsToCurrentThread());
    std::unique_ptr<GpuVideoAcceleratorFactories::ScopedGLContextLock> lock(
        gpu_factories_->GetGLContextLock());
    CHECK(lock);
    gpu::gles2::GLES2Interface* gl = lock->ContextGL();

    gl->GenTextures(1, &texture_);
    gl->BindTexture(GL_TEXTURE_2D, texture_);
    image_id_ = gl->CreateGpuMemoryBufferImageCHROMIUM(1, 1, GL_RGBA,
                                                       GL_READ_WRITE_CHROMIUM);
    CHECK(image_id_);
    gl->BindTexImage2DCHROMIUM(GL_TEXTURE_2D, image_id_);

    gl->GenMailboxCHROMIUM(mailbox_.name);
    gl->ProduceTextureDirectCHROMIUM(texture_, GL_TEXTURE_2D, mailbox_.name);

    const GLuint64 fence_sync = gl->InsertFenceSyncCHROMIUM();
    gl->ShallowFlushCHROMIUM();
    gl->GenSyncTokenCHROMIUM(fence_sync, sync_token_.GetData());
  }
}

VideoOverlayFactory::~VideoOverlayFactory() {
  if (texture_) {
    DCHECK(gpu_factories_->GetTaskRunner()->BelongsToCurrentThread());
    std::unique_ptr<GpuVideoAcceleratorFactories::ScopedGLContextLock> lock(
        gpu_factories_->GetGLContextLock());
    CHECK(lock);
    gpu::gles2::GLES2Interface* gl = lock->ContextGL();
    gl->BindTexture(GL_TEXTURE_2D, texture_);
    gl->ReleaseTexImage2DCHROMIUM(GL_TEXTURE_2D, image_id_);
    gl->DeleteTextures(1, &texture_);
    gl->DestroyImageCHROMIUM(image_id_);
  }
}

scoped_refptr<VideoFrame> VideoOverlayFactory::CreateFrame(
    const gfx::Size& size) {
  // No texture => audio device.  size empty => video has one dimension = 0.
  // Dimension 0 case triggers a DCHECK later on in TextureMailbox if we push
  // through the overlay path.
  if (!texture_ || size.IsEmpty()) {
    DVLOG(1) << "Create black frame " << size.width() << "x" << size.height();
    return VideoFrame::CreateBlackFrame(gfx::Size(1, 1));
  }

  DCHECK(gpu_factories_->GetTaskRunner()->BelongsToCurrentThread());
  DVLOG(1) << "Create hole frame " << size.width() << "x" << size.height();
  gpu::MailboxHolder holders[VideoFrame::kMaxPlanes] = {
      gpu::MailboxHolder(mailbox_, sync_token_, GL_TEXTURE_2D)};
  scoped_refptr<VideoFrame> frame = VideoFrame::WrapNativeTextures(
      PIXEL_FORMAT_XRGB, holders, VideoFrame::ReleaseMailboxCB(),
      size,                // coded_size
      gfx::Rect(size),     // visible rect
      size,                // natural size
      base::TimeDelta());  // timestamp
  CHECK(frame);
  frame->metadata()->SetBoolean(VideoFrameMetadata::ALLOW_OVERLAY, true);
  return frame;
}

}  // namespace media
