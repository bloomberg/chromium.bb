// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/media/avda_codec_image.h"

#include <string.h>

#include "base/metrics/histogram_macros.h"
#include "content/common/gpu/media/avda_shared_state.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/context_state.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "ui/gl/android/surface_texture.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/scoped_make_current.h"

namespace content {

AVDACodecImage::AVDACodecImage(
    const scoped_refptr<AVDASharedState>& shared_state,
    media::VideoCodecBridge* codec,
    const base::WeakPtr<gpu::gles2::GLES2Decoder>& decoder,
    const scoped_refptr<gfx::SurfaceTexture>& surface_texture)
    : shared_state_(shared_state),
      codec_buffer_index_(kInvalidCodecBufferIndex),
      media_codec_(codec),
      decoder_(decoder),
      surface_texture_(surface_texture),
      detach_surface_texture_on_destruction_(false),
      texture_(0),
      need_shader_info_(true),
      texmatrix_uniform_location_(-1) {
  memset(gl_matrix_, 0, sizeof(gl_matrix_));
  gl_matrix_[0] = gl_matrix_[5] = gl_matrix_[10] = gl_matrix_[15] = 1.0f;
}

AVDACodecImage::~AVDACodecImage() {}

void AVDACodecImage::Destroy(bool have_context) {}

gfx::Size AVDACodecImage::GetSize() {
  return size_;
}

unsigned AVDACodecImage::GetInternalFormat() {
  return GL_RGBA;
}

bool AVDACodecImage::BindTexImage(unsigned target) {
  return false;
}

void AVDACodecImage::ReleaseTexImage(unsigned target) {}

bool AVDACodecImage::CopyTexImage(unsigned target) {
  if (!surface_texture_)
    return false;

  if (target != GL_TEXTURE_EXTERNAL_OES)
    return false;

  // Verify that the currently bound texture is the right one.  If we're not
  // copying to a Texture that shares our service_id, then we can't do much.
  // This will force a copy.
  // TODO(liberato): Fall back to a copy that uses the texture matrix.
  GLint bound_service_id = 0;
  glGetIntegerv(GL_TEXTURE_BINDING_EXTERNAL_OES, &bound_service_id);
  if (bound_service_id != shared_state_->surface_texture_service_id())
    return false;

  // Attach the surface texture to our GL context if needed.
  if (!shared_state_->surface_texture_is_attached())
    AttachSurfaceTextureToContext();

  // Make sure that we have the right image in the front buffer.
  UpdateSurfaceTexture();

  InstallTextureMatrix();

  // TODO(liberato): Handle the texture matrix properly.
  // Either we can update the shader with it or we can move all of the logic
  // to updateTexImage() to the right place in the cc to send it to the shader.
  // For now, we just skip it.  crbug.com/530681

  // By setting image state to UNBOUND instead of COPIED we ensure that
  // CopyTexImage() is called each time the surface texture is used for drawing.
  // It would be nice if we could do this via asking for the currently bound
  // Texture, but the active unit never seems to change.
  texture_->SetLevelImage(GL_TEXTURE_EXTERNAL_OES, 0, this,
                          gpu::gles2::Texture::UNBOUND);

  return true;
}

bool AVDACodecImage::CopyTexSubImage(unsigned target,
                                     const gfx::Point& offset,
                                     const gfx::Rect& rect) {
  return false;
}

bool AVDACodecImage::ScheduleOverlayPlane(gfx::AcceleratedWidget widget,
                                          int z_order,
                                          gfx::OverlayTransform transform,
                                          const gfx::Rect& bounds_rect,
                                          const gfx::RectF& crop_rect) {
  // This should only be called when we're rendering to a SurfaceView.
  if (surface_texture_) {
    DVLOG(1) << "Invalid call to ScheduleOverlayPlane; this image is "
                "SurfaceTexture backed.";
    return false;
  }

  if (codec_buffer_index_ != kInvalidCodecBufferIndex) {
    media_codec_->ReleaseOutputBuffer(codec_buffer_index_, true);
    codec_buffer_index_ = kInvalidCodecBufferIndex;
  }
  return true;
}

void AVDACodecImage::OnMemoryDump(base::trace_event::ProcessMemoryDump* pmd,
                                  uint64_t process_tracing_id,
                                  const std::string& dump_name) {}

void AVDACodecImage::UpdateSurfaceTexture() {
  DCHECK(surface_texture_);

  // Render via the media codec if needed.
  if (codec_buffer_index_ == kInvalidCodecBufferIndex || !media_codec_)
    return;

  // The decoder buffer is still pending.
  // This must be synchronous, so wait for OnFrameAvailable.
  media_codec_->ReleaseOutputBuffer(codec_buffer_index_, true);
  {
    SCOPED_UMA_HISTOGRAM_TIMER("Media.AvdaCodecImage.WaitTimeForFrame");
    shared_state_->WaitForFrameAvailable();
  }

  // Don't bother to check if we're rendered again.
  codec_buffer_index_ = kInvalidCodecBufferIndex;

  // Swap the rendered image to the front.
  scoped_ptr<ui::ScopedMakeCurrent> scoped_make_current;
  if (!shared_state_->context()->IsCurrent(NULL)) {
    scoped_make_current.reset(new ui::ScopedMakeCurrent(
        shared_state_->context(), shared_state_->surface()));
  }
  surface_texture_->UpdateTexImage();

  // Helpfully, this is already column major.
  surface_texture_->GetTransformMatrix(gl_matrix_);
}

void AVDACodecImage::SetMediaCodecBufferIndex(int buffer_index) {
  codec_buffer_index_ = buffer_index;
}

int AVDACodecImage::GetMediaCodecBufferIndex() const {
  return codec_buffer_index_;
}

void AVDACodecImage::SetSize(const gfx::Size& size) {
  size_ = size;
}

void AVDACodecImage::SetMediaCodec(media::MediaCodecBridge* codec) {
  media_codec_ = codec;
}

void AVDACodecImage::SetTexture(gpu::gles2::Texture* texture) {
  texture_ = texture;
}

void AVDACodecImage::AttachSurfaceTextureToContext() {
  DCHECK(surface_texture_);

  // Attach the surface texture to the first context we're bound on, so that
  // no context switch is needed later.
  glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  // The surface texture is already detached, so just attach it.
  // We could do this earlier, but SurfaceTexture has context affinity, and we
  // don't want to require a context switch.
  surface_texture_->AttachToGLContext();
  shared_state_->did_attach_surface_texture();
}

void AVDACodecImage::InstallTextureMatrix() {
  DCHECK(surface_texture_);

  // glUseProgram() has been run already -- just modify the uniform.
  // Updating this via VideoFrameProvider::Client::DidUpdateMatrix() would
  // be a better solution, except that we'd definitely miss a frame at this
  // point in drawing.
  // Our current method assumes that we'll end up being a stream resource,
  // and that the program has a texMatrix uniform that does what we want.
  if (need_shader_info_) {
    GLint program_id = -1;
    glGetIntegerv(GL_CURRENT_PROGRAM, &program_id);

    if (program_id >= 0) {
      // This is memorized from cc/output/shader.cc .
      const char* uniformName = "texMatrix";
      texmatrix_uniform_location_ =
          glGetUniformLocation(program_id, uniformName);
      DCHECK(texmatrix_uniform_location_ != -1);
    }

    // Only try once.
    need_shader_info_ = false;
  }

  if (texmatrix_uniform_location_ >= 0) {
    glUniformMatrix4fv(texmatrix_uniform_location_, 1, false, gl_matrix_);
  }
}

}  // namespace content
