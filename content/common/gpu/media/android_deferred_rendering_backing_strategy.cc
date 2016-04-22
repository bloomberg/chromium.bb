// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/media/android_deferred_rendering_backing_strategy.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include "base/android/build_info.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "content/common/gpu/media/avda_codec_image.h"
#include "content/common/gpu/media/avda_return_on_failure.h"
#include "content/common/gpu/media/avda_shared_state.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/gl_stream_texture_image.h"
#include "gpu/command_buffer/service/gles2_cmd_copy_texture_chromium.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/ipc/common/gpu_surface_lookup.h"
#include "gpu/ipc/service/gpu_channel.h"
#include "ui/gl/android/surface_texture.h"
#include "ui/gl/egl_util.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/scoped_binders.h"
#include "ui/gl/scoped_make_current.h"

namespace content {

AndroidDeferredRenderingBackingStrategy::
    AndroidDeferredRenderingBackingStrategy(AVDAStateProvider* state_provider)
    : state_provider_(state_provider), media_codec_(nullptr) {}

AndroidDeferredRenderingBackingStrategy::
    ~AndroidDeferredRenderingBackingStrategy() {}

gfx::ScopedJavaSurface AndroidDeferredRenderingBackingStrategy::Initialize(
    int surface_view_id) {
  shared_state_ = new AVDASharedState();

  // Create a texture for the SurfaceTexture to use.  We don't attach it here
  // so that it gets attached in the compositor gl context in the common case.
  GLuint service_id = 0;
  glGenTextures(1, &service_id);
  DCHECK(service_id);
  shared_state_->set_surface_texture_service_id(service_id);

  gfx::ScopedJavaSurface surface;
  if (surface_view_id != media::VideoDecodeAccelerator::Config::kNoSurfaceID) {
    surface = gpu::GpuSurfaceLookup::GetInstance()->AcquireJavaSurface(
        surface_view_id);
  } else {
    if (DoesSurfaceTextureDetachWork()) {
      // Create a detached SurfaceTexture. Detaching it will silently fail to
      // delete texture 0.
      surface_texture_ = gfx::SurfaceTexture::Create(0);
      surface_texture_->DetachFromGLContext();
    } else {
      // Detach doesn't work so well on all platforms.  Just attach the
      // SurfaceTexture here, and probably context switch later.
      surface_texture_ = gfx::SurfaceTexture::Create(service_id);
      shared_state_->DidAttachSurfaceTexture();
    }
    surface = gfx::ScopedJavaSurface(surface_texture_.get());
  }

  return surface;
}

void AndroidDeferredRenderingBackingStrategy::Cleanup(
    bool have_context,
    const AndroidVideoDecodeAccelerator::OutputBufferMap& buffers) {
  // If we failed before Initialize, then do nothing.
  if (!shared_state_)
    return;

  // Make sure that no PictureBuffer textures refer to the SurfaceTexture or to
  // the service_id that we created for it.
  for (const std::pair<int, media::PictureBuffer>& entry : buffers)
    SetImageForPicture(entry.second, nullptr);

  // If we're rendering to a SurfaceTexture we can make a copy of the current
  // front buffer so that the PictureBuffer textures are still valid.
  if (surface_texture_ && have_context && ShouldCopyPictures())
    CopySurfaceTextureToPictures(buffers);

  // Now that no AVDACodecImages refer to the SurfaceTexture's texture, delete
  // the texture name.
  GLuint service_id = shared_state_->surface_texture_service_id();
  if (service_id > 0 && have_context)
    glDeleteTextures(1, &service_id);
}

scoped_refptr<gfx::SurfaceTexture>
AndroidDeferredRenderingBackingStrategy::GetSurfaceTexture() const {
  return surface_texture_;
}

uint32_t AndroidDeferredRenderingBackingStrategy::GetTextureTarget() const {
  // If we're using a surface texture, then we need an external texture target
  // to sample from it.  If not, then we'll use 2D transparent textures to draw
  // a transparent hole through which to see the SurfaceView.  This is normally
  // needed only for the devtools inspector, since the overlay mechanism handles
  // it otherwise.
  return surface_texture_ ? GL_TEXTURE_EXTERNAL_OES : GL_TEXTURE_2D;
}

gfx::Size AndroidDeferredRenderingBackingStrategy::GetPictureBufferSize()
    const {
  // For SurfaceView, request a 1x1 2D texture to reduce memory during
  // initialization.  For SurfaceTexture, allocate a picture buffer that is the
  // actual frame size.  Note that it will be an external texture anyway, so it
  // doesn't allocate an image of that size.  However, it's still important to
  // get the coded size right, so that VideoLayerImpl doesn't try to scale the
  // texture when building the quad for it.
  return surface_texture_ ? state_provider_->GetSize() : gfx::Size(1, 1);
}

void AndroidDeferredRenderingBackingStrategy::SetImageForPicture(
    const media::PictureBuffer& picture_buffer,
    const scoped_refptr<gpu::gles2::GLStreamTextureImage>& image) {
  gpu::gles2::TextureRef* texture_ref =
      state_provider_->GetTextureForPicture(picture_buffer);
  RETURN_IF_NULL(texture_ref);

  gpu::gles2::TextureManager* texture_manager =
      state_provider_->GetGlDecoder()->GetContextGroup()->texture_manager();
  RETURN_IF_NULL(texture_manager);

  if (image) {
    // Also set the parameters for the level if we're not clearing
    // the image.
    const gfx::Size size = state_provider_->GetSize();
    texture_manager->SetLevelInfo(texture_ref, GetTextureTarget(), 0, GL_RGBA,
                                  size.width(), size.height(), 1, 0, GL_RGBA,
                                  GL_UNSIGNED_BYTE, gfx::Rect());

    // Override the texture's service_id, so that it will use the one that
    // will be / is attached to the SurfaceTexture.
    DCHECK(shared_state_->surface_texture_service_id());
    texture_ref->texture()->SetUnownedServiceId(
        shared_state_->surface_texture_service_id());

    static_cast<AVDACodecImage*>(image.get())
        ->SetTexture(texture_ref->texture());
  } else {
    // Clear the unowned service_id, so that this texture is no longer going
    // to depend on the surface texture at all.
    texture_ref->texture()->SetUnownedServiceId(0);
  }

  // For SurfaceTexture we set the image to UNBOUND so that the implementation
  // will call CopyTexImage, which is where AVDACodecImage updates the
  // SurfaceTexture to the right frame.
  // For SurfaceView we set the image to be BOUND because ScheduleOverlayPlane
  // expects it. If something tries to sample from this texture it won't work,
  // but there's no way to sample from a SurfaceView anyway, so it doesn't
  // matter. The only way to use this texture is to schedule it as an overlay.
  const gpu::gles2::Texture::ImageState image_state =
      surface_texture_ ? gpu::gles2::Texture::UNBOUND
                       : gpu::gles2::Texture::BOUND;
  texture_manager->SetLevelStreamTextureImage(texture_ref, GetTextureTarget(),
                                              0, image.get(), image_state);
}

void AndroidDeferredRenderingBackingStrategy::UseCodecBufferForPictureBuffer(
    int32_t codec_buf_index,
    const media::PictureBuffer& picture_buffer) {
  // Make sure that the decoder is available.
  RETURN_IF_NULL(state_provider_->GetGlDecoder());

  // Notify the AVDACodecImage for picture_buffer that it should use the
  // decoded buffer codec_buf_index to render this frame.
  AVDACodecImage* avda_image =
      shared_state_->GetImageForPicture(picture_buffer.id());
  RETURN_IF_NULL(avda_image);
  DCHECK_EQ(avda_image->GetMediaCodecBufferIndex(), -1);
  // Note that this is not a race, since we do not re-use a PictureBuffer
  // until after the CC is done drawing it.
  avda_image->SetMediaCodecBufferIndex(codec_buf_index);
  avda_image->SetSize(state_provider_->GetSize());
}

void AndroidDeferredRenderingBackingStrategy::AssignOnePictureBuffer(
    const media::PictureBuffer& picture_buffer,
    bool have_context) {
  // Attach a GLImage to each texture that will use the surface texture.
  // We use a refptr here in case SetImageForPicture fails.
  scoped_refptr<gpu::gles2::GLStreamTextureImage> gl_image =
      new AVDACodecImage(picture_buffer.id(), shared_state_, media_codec_,
                         state_provider_->GetGlDecoder(), surface_texture_);
  SetImageForPicture(picture_buffer, gl_image);

  if (!surface_texture_ && have_context) {
    // To make devtools work, we're using a 2D texture.  Make it transparent,
    // so that it draws a hole for the SV to show through.  This is only
    // because devtools draws and reads back, which skips overlay processing.
    // It's unclear why devtools renders twice -- once normally, and once
    // including a readback layer.  The result is that the device screen
    // flashes as we alternately draw the overlay hole and this texture,
    // unless we make the texture transparent.
    static const uint8_t rgba[] = {0, 0, 0, 0};
    const gfx::Size size(1, 1);
    DCHECK_LE(1u, picture_buffer.texture_ids().size());
    glBindTexture(GL_TEXTURE_2D, picture_buffer.texture_ids()[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size.width(), size.height(), 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, rgba);
  }
}

void AndroidDeferredRenderingBackingStrategy::ReleaseCodecBufferForPicture(
    const media::PictureBuffer& picture_buffer) {
  AVDACodecImage* avda_image =
      shared_state_->GetImageForPicture(picture_buffer.id());
  RETURN_IF_NULL(avda_image);

  // See if there is a media codec buffer still attached to this image.
  const int32_t codec_buffer = avda_image->GetMediaCodecBufferIndex();

  if (codec_buffer >= 0) {
    // PictureBuffer wasn't displayed, so release the buffer.
    media_codec_->ReleaseOutputBuffer(codec_buffer, false);
    avda_image->SetMediaCodecBufferIndex(-1);
  }
}

void AndroidDeferredRenderingBackingStrategy::ReuseOnePictureBuffer(
    const media::PictureBuffer& picture_buffer) {
  // At this point, the CC must be done with the picture.  We can't really
  // check for that here directly.  it's guaranteed in gpu_video_decoder.cc,
  // when it waits on the sync point before releasing the mailbox.  That sync
  // point is inserted by destroying the resource in VideoLayerImpl::DidDraw.
  ReleaseCodecBufferForPicture(picture_buffer);
}

void AndroidDeferredRenderingBackingStrategy::CodecChanged(
    media::VideoCodecBridge* codec) {
  media_codec_ = codec;
  shared_state_->CodecChanged(codec);
}

void AndroidDeferredRenderingBackingStrategy::OnFrameAvailable() {
  shared_state_->SignalFrameAvailable();
}

bool AndroidDeferredRenderingBackingStrategy::ArePicturesOverlayable() {
  // SurfaceView frames are always overlayable because that's the only way to
  // display them.
  return !surface_texture_;
}

void AndroidDeferredRenderingBackingStrategy::UpdatePictureBufferSize(
    media::PictureBuffer* picture_buffer,
    const gfx::Size& new_size) {
  // This strategy uses EGL images which manage the texture size for us.  We
  // simply update the PictureBuffer meta-data and leave the texture as-is.
  picture_buffer->set_size(new_size);
}

void AndroidDeferredRenderingBackingStrategy::CopySurfaceTextureToPictures(
    const AndroidVideoDecodeAccelerator::OutputBufferMap& buffers) {
  DVLOG(3) << __FUNCTION__;

  // Don't try to copy if the SurfaceTexture was never attached because that
  // means it was never updated.
  if (!shared_state_->surface_texture_is_attached())
    return;

  gpu::gles2::GLES2Decoder* gl_decoder = state_provider_->GetGlDecoder().get();
  if (!gl_decoder)
    return;

  const gfx::Size size = state_provider_->GetSize();

  // Create a 2D texture to hold a copy of the SurfaceTexture's front buffer.
  GLuint tmp_texture_id;
  glGenTextures(1, &tmp_texture_id);
  {
    gfx::ScopedTextureBinder texture_binder(GL_TEXTURE_2D, tmp_texture_id);
    // The target texture's size will exactly match the source.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size.width(), size.height(), 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
  }

  float transform_matrix[16];
  surface_texture_->GetTransformMatrix(transform_matrix);

  gpu::CopyTextureCHROMIUMResourceManager copier;
  copier.Initialize(
      gl_decoder,
      gl_decoder->GetContextGroup()->feature_info()->feature_flags());
  copier.DoCopyTextureWithTransform(gl_decoder, GL_TEXTURE_EXTERNAL_OES,
                                    shared_state_->surface_texture_service_id(),
                                    GL_TEXTURE_2D, tmp_texture_id, size.width(),
                                    size.height(), true, false, false,
                                    transform_matrix);

  // Create an EGLImage from the 2D texture we just copied into. By associating
  // the EGLImage with the PictureBuffer textures they will remain valid even
  // after we delete the 2D texture and EGLImage.
  const EGLImageKHR egl_image = eglCreateImageKHR(
      gfx::GLSurfaceEGL::GetHardwareDisplay(), eglGetCurrentContext(),
      EGL_GL_TEXTURE_2D_KHR, reinterpret_cast<EGLClientBuffer>(tmp_texture_id),
      nullptr /* attrs */);

  glDeleteTextures(1, &tmp_texture_id);
  DCHECK_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());

  if (egl_image == EGL_NO_IMAGE_KHR) {
    DLOG(ERROR) << "Failed creating EGLImage: " << ui::GetLastEGLErrorString();
    return;
  }

  for (const std::pair<int, media::PictureBuffer>& entry : buffers) {
    gpu::gles2::TextureRef* texture_ref =
        state_provider_->GetTextureForPicture(entry.second);
    if (!texture_ref)
      continue;
    gfx::ScopedTextureBinder texture_binder(
        GL_TEXTURE_EXTERNAL_OES, texture_ref->texture()->service_id());
    glEGLImageTargetTexture2DOES(GL_TEXTURE_EXTERNAL_OES, egl_image);
    DCHECK_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
  }

  EGLBoolean result =
      eglDestroyImageKHR(gfx::GLSurfaceEGL::GetHardwareDisplay(), egl_image);
  if (result == EGL_FALSE) {
    DLOG(ERROR) << "Error destroying EGLImage: "
                << ui::GetLastEGLErrorString();
  }
}

bool AndroidDeferredRenderingBackingStrategy::DoesSurfaceTextureDetachWork()
    const {
  bool surface_texture_detach_works = true;
  if (gpu::gles2::GLES2Decoder* gl_decoder =
          state_provider_->GetGlDecoder().get()) {
    if (gpu::gles2::ContextGroup* group = gl_decoder->GetContextGroup()) {
      if (gpu::gles2::FeatureInfo* feature_info = group->feature_info()) {
        surface_texture_detach_works =
            !feature_info->workarounds().surface_texture_cant_detach;
      }
    }
  }

  // As a special case, the MicroMax A114 doesn't get the workaround, even
  // though it should.  Hardcode it here until we get a device and figure out
  // why.  crbug.com/591600
  if (base::android::BuildInfo::GetInstance()->sdk_int() <= 18) {  // JB
    const std::string brand(
        base::ToLowerASCII(base::android::BuildInfo::GetInstance()->brand()));
    if (brand == "micromax") {
      const std::string model(
          base::ToLowerASCII(base::android::BuildInfo::GetInstance()->model()));
      if (model.find("a114") != std::string::npos)
        surface_texture_detach_works = false;
    }
  }

  return surface_texture_detach_works;
}

bool AndroidDeferredRenderingBackingStrategy::ShouldCopyPictures() const {
  // Mali + <= KitKat crashes when we try to do this.  We don't know if it's
  // due to detaching a surface texture, but it's the same set of devices.
  if (!DoesSurfaceTextureDetachWork())
      return false;

  // Other devices are unreliable for other reasons (e.g., EGLImage).
  if (gpu::gles2::GLES2Decoder* gl_decoder =
          state_provider_->GetGlDecoder().get()) {
    if (gpu::gles2::ContextGroup* group = gl_decoder->GetContextGroup()) {
      if (gpu::gles2::FeatureInfo* feature_info = group->feature_info()) {
          return !feature_info->workarounds().avda_dont_copy_pictures;
      }
    }
  }

  // Assume so.
  return true;
}

}  // namespace content
