// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/media/avda_codec_image.h"

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
      codec_buffer_index_(-1),
      media_codec_(codec),
      decoder_(decoder),
      surface_texture_(surface_texture),
      detach_surface_texture_on_destruction_(false) {}

AVDACodecImage::~AVDACodecImage() {}

void AVDACodecImage::Destroy(bool have_context) {
  // If the SurfaceTexture is using our texture, then detach from it.
  if (detach_surface_texture_on_destruction_) {
    // We don't really care if we have no context, since it doesn't
    // matter if the texture is destroyed here or not.  As long as the
    // surface texture doesn't try to delete this handle later (after
    // it might have been reused), it's fine.  Somebody else will delete
    // our texture when the picture buffer is destroyed.
    surface_texture_->DetachFromGLContext();
    shared_state_->set_surface_texture_service_id(0);
  }
}

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
  // Have we bound the SurfaceTexture's texture handle to the active
  // texture unit yet?
  bool bound_texture = false;

  // Attach the surface texture to our GL context if needed.
  if (!shared_state_->surface_texture_service_id()) {
    AttachSurfaceTextureToContext();
    bound_texture = true;
  }

  // Make sure that we have the right image in the front buffer.
  bound_texture |= UpdateSurfaceTexture();

  // Sneakily bind the ST texture handle in the real GL context.
  // If we called UpdateTexImage() to update the ST front buffer, then we can
  // skip this.  Since one draw/frame is the common case, we optimize for it.
  if (!bound_texture)
    glBindTexture(GL_TEXTURE_EXTERNAL_OES,
                  shared_state_->surface_texture_service_id());

  // TODO(liberato): Handle the texture matrix properly.
  // Either we can update the shader with it or we can move all of the logic
  // to updateTexImage() to the right place in the cc to send it to the shader.
  // For now, we just skip it.  crbug.com/530681

  gpu::gles2::TextureManager* texture_manager =
      decoder_->GetContextGroup()->texture_manager();
  gpu::gles2::Texture* texture =
      texture_manager->GetTextureForServiceId(
          shared_state_->surface_texture_service_id());
  if (texture) {
    // By setting image state to UNBOUND instead of COPIED we ensure that
    // CopyTexImage() is called each time the surface texture is used for
    // drawing.
    texture->SetLevelImage(GL_TEXTURE_EXTERNAL_OES, 0, this,
                           gpu::gles2::Texture::UNBOUND);
  }

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
  return false;
}

void AVDACodecImage::OnMemoryDump(base::trace_event::ProcessMemoryDump* pmd,
                                  uint64_t process_tracing_id,
                                  const std::string& dump_name) {}

bool AVDACodecImage::UpdateSurfaceTexture() {
  // Render via the media codec if needed.
  if (codec_buffer_index_ > -1 && media_codec_) {
    // We have been given a codec buffer to render, so render it.
    // We might want to ask the avda to release any buffers that come
    // before us without rendering, just for good measure.  However,
    // to prevent doing lots of work on the drawing path, we skip it.

    // The decoder buffer was still pending.
    // This must be synchronous.
    media_codec_->ReleaseOutputBuffer(codec_buffer_index_, true);

    // Don't bother to check if we're rendered again.
    codec_buffer_index_ = -1;

    // Swap the rendered image to the front.
    surface_texture_->UpdateTexImage();

    // UpdateTexImage() binds the ST's texture.
    return true;
  }

  return false;
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

void AVDACodecImage::AttachSurfaceTextureToContext() {
  GLint surface_texture_service_id;
  // Use the PictureBuffer's texture.  We could also generate a new texture
  // here, but cleaning it up is problematic.
  glGetIntegerv(GL_TEXTURE_BINDING_EXTERNAL_OES, &surface_texture_service_id);
  DCHECK(surface_texture_service_id);

  // Attach to our service id.
  glBindTexture(GL_TEXTURE_EXTERNAL_OES, surface_texture_service_id);
  glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  // The surface texture is already detached, so just attach it.
  surface_texture_->AttachToGLContext();
  shared_state_->set_surface_texture_service_id(surface_texture_service_id);
  detach_surface_texture_on_destruction_ = true;

  // We do not restore the GL state here.
}

}  // namespace content
