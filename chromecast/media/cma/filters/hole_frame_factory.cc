// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/filters/hole_frame_factory.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "chromecast/base/chromecast_switches.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "media/base/video_frame.h"
#include "media/renderers/gpu_video_accelerator_factories.h"

namespace chromecast {
namespace media {

HoleFrameFactory::HoleFrameFactory(
    const scoped_refptr<::media::GpuVideoAcceleratorFactories>& gpu_factories)
    : gpu_factories_(gpu_factories),
      texture_(0),
      image_id_(0),
      use_legacy_hole_punching_(
          base::CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kEnableLegacyHolePunching)) {
  if (!use_legacy_hole_punching_) {
    gpu::gles2::GLES2Interface* gl = gpu_factories_->GetGLES2Interface();
    CHECK(gl);

    gl->GenTextures(1, &texture_);
    gl->BindTexture(GL_TEXTURE_2D, texture_);
    image_id_ = gl->CreateGpuMemoryBufferImageCHROMIUM(1, 1, GL_RGBA,
                                                       GL_SCANOUT_CHROMIUM);
    gl->BindTexImage2DCHROMIUM(GL_TEXTURE_2D, image_id_);

    gl->GenMailboxCHROMIUM(mailbox_.name);
    gl->ProduceTextureDirectCHROMIUM(texture_, GL_TEXTURE_2D, mailbox_.name);
  }
}

HoleFrameFactory::~HoleFrameFactory() {
  if (texture_ != 0) {
    gpu::gles2::GLES2Interface* gl = gpu_factories_->GetGLES2Interface();
    gl->BindTexture(GL_TEXTURE_2D, texture_);
    gl->ReleaseTexImage2DCHROMIUM(GL_TEXTURE_2D, image_id_);
    gl->DeleteTextures(1, &texture_);
    gl->DestroyImageCHROMIUM(image_id_);
  }
}

scoped_refptr<::media::VideoFrame> HoleFrameFactory::CreateHoleFrame(
    const gfx::Size& size) {
  if (use_legacy_hole_punching_) {
#if defined(VIDEO_HOLE)
    return ::media::VideoFrame::CreateHoleFrame(size);
#endif
    NOTREACHED();
  }

  scoped_refptr<::media::VideoFrame> frame =
      ::media::VideoFrame::WrapNativeTexture(
          ::media::VideoFrame::XRGB,
          gpu::MailboxHolder(mailbox_, GL_TEXTURE_2D, 0),
          ::media::VideoFrame::ReleaseMailboxCB(),
          size,                // coded_size
          gfx::Rect(size),     // visible rect
          size,                // natural size
          base::TimeDelta());  // timestamp
  frame->metadata()->SetBoolean(::media::VideoFrameMetadata::ALLOW_OVERLAY,
                                true);
  return frame;
}

}  // namespace media
}  // namespace chromecast
