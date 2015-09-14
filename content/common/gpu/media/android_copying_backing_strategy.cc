// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/media/android_copying_backing_strategy.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "content/common/gpu/media/avda_return_on_failure.h"
#include "gpu/command_buffer/service/gles2_cmd_copy_texture_chromium.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "media/base/limits.h"
#include "media/video/picture.h"
#include "ui/gl/android/surface_texture.h"
#include "ui/gl/gl_bindings.h"

namespace content {

// TODO(liberato): It is unclear if we have an issue with deadlock during
// playback if we lower this.  Previously (crbug.com/176036), a deadlock
// could occur during preroll.  More recent tests have shown some
// instability with kNumPictureBuffers==2 with similar symptoms
// during playback.  crbug.com/:531588 .
enum { kNumPictureBuffers = media::limits::kMaxVideoFrames + 1 };

const static GLfloat kIdentityMatrix[16] = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f,
                                            0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f,
                                            0.0f, 0.0f, 0.0f, 1.0f};

AndroidCopyingBackingStrategy::AndroidCopyingBackingStrategy()
    : state_provider_(nullptr) {}

AndroidCopyingBackingStrategy::~AndroidCopyingBackingStrategy() {}

void AndroidCopyingBackingStrategy::SetStateProvider(
    AndroidVideoDecodeAcceleratorStateProvider* state_provider) {
  state_provider_ = state_provider;
}

void AndroidCopyingBackingStrategy::Cleanup() {
  DCHECK(state_provider_->ThreadChecker().CalledOnValidThread());
  if (copier_)
    copier_->Destroy();
}

uint32 AndroidCopyingBackingStrategy::GetNumPictureBuffers() const {
  return kNumPictureBuffers;
}

uint32 AndroidCopyingBackingStrategy::GetTextureTarget() const {
  return GL_TEXTURE_2D;
}

void AndroidCopyingBackingStrategy::AssignCurrentSurfaceToPictureBuffer(
    int32 codec_buf_index,
    const media::PictureBuffer& picture_buffer) {
  // Make sure that the decoder is available.
  RETURN_ON_FAILURE(state_provider_, state_provider_->GetGlDecoder(),
                    "Failed to get gles2 decoder instance.", ILLEGAL_STATE);

  // Render the codec buffer into |surface_texture_|, and switch it to be
  // the front buffer.
  // This ignores the emitted ByteBuffer and instead relies on rendering to
  // the codec's SurfaceTexture and then copying from that texture to the
  // client's PictureBuffer's texture.  This means that each picture's data
  // is written three times: once to the ByteBuffer, once to the
  // SurfaceTexture, and once to the client's texture.  It would be nicer to
  // either:
  // 1) Render directly to the client's texture from MediaCodec (one write);
  //    or
  // 2) Upload the ByteBuffer to the client's texture (two writes).
  // Unfortunately neither is possible:
  // 1) MediaCodec's use of SurfaceTexture is a singleton, and the texture
  //    written to can't change during the codec's lifetime.  b/11990461
  // 2) The ByteBuffer is likely to contain the pixels in a vendor-specific,
  //    opaque/non-standard format.  It's not possible to negotiate the
  //    decoder to emit a specific colorspace, even using HW CSC.  b/10706245
  // So, we live with these two extra copies per picture :(
  {
    TRACE_EVENT0("media", "AVDA::ReleaseOutputBuffer");
    state_provider_->GetMediaCodec()->ReleaseOutputBuffer(codec_buf_index,
                                                          true);
  }

  gfx::SurfaceTexture* surface_texture = state_provider_->GetSurfaceTexture();
  {
    TRACE_EVENT0("media", "AVDA::UpdateTexImage");
    surface_texture->UpdateTexImage();
  }

  float transfrom_matrix[16];
  surface_texture->GetTransformMatrix(transfrom_matrix);

  uint32 picture_buffer_texture_id = picture_buffer.texture_id();

  // Defer initializing the CopyTextureCHROMIUMResourceManager until it is
  // needed because it takes 10s of milliseconds to initialize.
  if (!copier_) {
    copier_.reset(new gpu::CopyTextureCHROMIUMResourceManager());
    copier_->Initialize(state_provider_->GetGlDecoder());
  }

  // Here, we copy |surface_texture_id_| to the picture buffer instead of
  // setting new texture to |surface_texture_| by calling attachToGLContext()
  // because:
  // 1. Once we call detachFrameGLContext(), it deletes the texture previous
  //    attached.
  // 2. SurfaceTexture requires us to apply a transform matrix when we show
  //    the texture.
  // TODO(hkuang): get the StreamTexture transform matrix in GPU process
  // instead of using default matrix crbug.com/226218.
  copier_->DoCopyTextureWithTransform(
      state_provider_->GetGlDecoder(), GL_TEXTURE_EXTERNAL_OES,
      state_provider_->GetSurfaceTextureId(), picture_buffer_texture_id,
      state_provider_->GetSize().width(), state_provider_->GetSize().height(),
      false, false, false, kIdentityMatrix);
}

}  // namespace content
