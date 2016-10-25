// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/avda_picture_buffer_manager.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include "base/android/build_info.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/gl_stream_texture_image.h"
#include "gpu/command_buffer/service/gles2_cmd_copy_texture_chromium.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/ipc/common/gpu_surface_lookup.h"
#include "gpu/ipc/service/gpu_channel.h"
#include "media/base/android/sdk_media_codec_bridge.h"
#include "media/gpu/avda_codec_image.h"
#include "media/gpu/avda_shared_state.h"
#include "ui/gl/android/surface_texture.h"
#include "ui/gl/egl_util.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/scoped_binders.h"
#include "ui/gl/scoped_make_current.h"

// If !|ptr|, log a message, post an error to |state_provider_|, and
// return an optional value.
#define RETURN_IF_NULL(ptr, ...)                                         \
  do {                                                                   \
    if (!(ptr)) {                                                        \
      DLOG(ERROR) << "Got null for " << #ptr;                            \
      state_provider_->PostError(FROM_HERE,                              \
                                 VideoDecodeAccelerator::ILLEGAL_STATE); \
      return __VA_ARGS__;                                                \
    }                                                                    \
  } while (0)

// Return nullptr if !|ptr|.
#define RETURN_NULL_IF_NULL(ptr) RETURN_IF_NULL(ptr, nullptr)

namespace media {
namespace {

// Creates a SurfaceTexture and attaches a new gl texture to it. |*service_id|
// is set to the new texture id.
scoped_refptr<gl::SurfaceTexture> CreateAttachedSurfaceTexture(
    base::WeakPtr<gpu::gles2::GLES2Decoder> gl_decoder,
    GLuint* service_id) {
  GLuint texture_id;
  glGenTextures(1, &texture_id);

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_EXTERNAL_OES, texture_id);
  glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  gl_decoder->RestoreTextureUnitBindings(0);
  gl_decoder->RestoreActiveTexture();
  DCHECK_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());

  *service_id = texture_id;
  // Previously, to reduce context switching, we used to create an unattached
  // SurfaceTexture and attach it lazily in the compositor's context. But that
  // was flaky because SurfaceTexture#detachFromGLContext() is buggy on a lot of
  // devices. Now we attach it to the current context, which means we might have
  // to context switch later to call updateTexImage(). Fortunately, if virtual
  // contexts are in use, we won't have to context switch.
  return gl::SurfaceTexture::Create(texture_id);
}

}  // namespace

// Handle OnFrameAvailable callbacks safely.  Since they occur asynchronously,
// we take care that the object that wants them still exists.  WeakPtrs cannot
// be used because OnFrameAvailable callbacks can occur on any thread. We also
// can't guarantee when the SurfaceTexture will quit sending callbacks to
// coordinate with the destruction of the AVDA and PictureBufferManager, so we
// have a separate object that the callback can own.
class AVDAPictureBufferManager::OnFrameAvailableHandler
    : public base::RefCountedThreadSafe<OnFrameAvailableHandler> {
 public:
  // We do not retain ownership of |listener|.  It must remain valid until after
  // ClearListener() is called.  This will register with |surface_texture| to
  // receive OnFrameAvailable callbacks.
  OnFrameAvailableHandler(AVDASharedState* listener,
                          gl::SurfaceTexture* surface_texture)
      : listener_(listener) {
    surface_texture->SetFrameAvailableCallbackOnAnyThread(
        base::Bind(&OnFrameAvailableHandler::OnFrameAvailable,
                   scoped_refptr<OnFrameAvailableHandler>(this)));
  }

  // Forget about |listener_|, which is required before one deletes it.
  // No further callbacks will happen once this completes.
  void ClearListener() {
    base::AutoLock lock(lock_);
    listener_ = nullptr;
  }

  // Notify the listener if there is one.
  void OnFrameAvailable() {
    base::AutoLock auto_lock(lock_);
    if (listener_)
      listener_->SignalFrameAvailable();
  }

 private:
  friend class base::RefCountedThreadSafe<OnFrameAvailableHandler>;

  ~OnFrameAvailableHandler() { DCHECK(!listener_); }

  // Protects changes to listener_.
  base::Lock lock_;

  // The AVDASharedState that wants the OnFrameAvailable callback.
  AVDASharedState* listener_;

  DISALLOW_COPY_AND_ASSIGN(OnFrameAvailableHandler);
};

AVDAPictureBufferManager::AVDAPictureBufferManager()
    : state_provider_(nullptr), media_codec_(nullptr) {}

AVDAPictureBufferManager::~AVDAPictureBufferManager() {}

gl::ScopedJavaSurface AVDAPictureBufferManager::Initialize(
    AVDAStateProvider* state_provider,
    int surface_view_id) {
  state_provider_ = state_provider;
  shared_state_ = new AVDASharedState();

  // Acquire the SurfaceView surface if given a valid id.
  if (surface_view_id != VideoDecodeAccelerator::Config::kNoSurfaceID) {
    return gpu::GpuSurfaceLookup::GetInstance()->AcquireJavaSurface(
        surface_view_id);
  }

  // Otherwise create a SurfaceTexture.
  GLuint service_id = 0;
  surface_texture_ = CreateAttachedSurfaceTexture(
      state_provider_->GetGlDecoder(), &service_id);
  if (surface_texture_) {
    on_frame_available_handler_ = new OnFrameAvailableHandler(
        shared_state_.get(), surface_texture_.get());
  }
  shared_state_->SetSurfaceTexture(surface_texture_, service_id);
  return gl::ScopedJavaSurface(surface_texture_.get());
}

void AVDAPictureBufferManager::Destroy(const PictureBufferMap& buffers) {
  // Do nothing if Initialize() has not been called.
  if (!shared_state_)
    return;

  // If we have an OnFrameAvailable handler, tell it that we no longer want
  // callbacks.
  if (on_frame_available_handler_)
    on_frame_available_handler_->ClearListener();

  ReleaseCodecBuffers(buffers);
  CodecChanged(nullptr);

  // Release the surface texture and any back buffers.  This will preserve the
  // front buffer, if any.
  if (surface_texture_)
    surface_texture_->ReleaseSurfaceTexture();
}

uint32_t AVDAPictureBufferManager::GetTextureTarget() const {
  // If we're using a surface texture, then we need an external texture target
  // to sample from it.  If not, then we'll use 2D transparent textures to draw
  // a transparent hole through which to see the SurfaceView.  This is normally
  // needed only for the devtools inspector, since the overlay mechanism handles
  // it otherwise.
  return surface_texture_ ? GL_TEXTURE_EXTERNAL_OES : GL_TEXTURE_2D;
}

gfx::Size AVDAPictureBufferManager::GetPictureBufferSize() const {
  // For SurfaceView, request a 1x1 2D texture to reduce memory during
  // initialization.  For SurfaceTexture, allocate a picture buffer that is the
  // actual frame size.  Note that it will be an external texture anyway, so it
  // doesn't allocate an image of that size.  However, it's still important to
  // get the coded size right, so that VideoLayerImpl doesn't try to scale the
  // texture when building the quad for it.
  return surface_texture_ ? state_provider_->GetSize() : gfx::Size(1, 1);
}

gpu::gles2::TextureRef* AVDAPictureBufferManager::GetTextureForPicture(
    const PictureBuffer& picture_buffer) {
  auto gles_decoder = state_provider_->GetGlDecoder();
  RETURN_NULL_IF_NULL(gles_decoder);
  RETURN_NULL_IF_NULL(gles_decoder->GetContextGroup());

  gpu::gles2::TextureManager* texture_manager =
      gles_decoder->GetContextGroup()->texture_manager();
  RETURN_NULL_IF_NULL(texture_manager);

  DCHECK_LE(1u, picture_buffer.client_texture_ids().size());
  gpu::gles2::TextureRef* texture_ref =
      texture_manager->GetTexture(picture_buffer.client_texture_ids()[0]);
  RETURN_NULL_IF_NULL(texture_ref);

  return texture_ref;
}

void AVDAPictureBufferManager::SetImageForPicture(
    const PictureBuffer& picture_buffer,
    const scoped_refptr<gpu::gles2::GLStreamTextureImage>& image) {
  gpu::gles2::TextureRef* texture_ref = GetTextureForPicture(picture_buffer);
  RETURN_IF_NULL(texture_ref);

  gpu::gles2::TextureManager* texture_manager =
      state_provider_->GetGlDecoder()->GetContextGroup()->texture_manager();
  RETURN_IF_NULL(texture_manager);

  // Default to zero which will clear the stream texture service id if one was
  // previously set.
  GLuint stream_texture_service_id = 0;
  if (image) {
    if (shared_state_->surface_texture_service_id() != 0) {
      // Override the Texture's service id, so that it will use the one that is
      // attached to the SurfaceTexture.
      stream_texture_service_id = shared_state_->surface_texture_service_id();
    }

    // Also set the parameters for the level if we're not clearing the image.
    const gfx::Size size = state_provider_->GetSize();
    texture_manager->SetLevelInfo(texture_ref, GetTextureTarget(), 0, GL_RGBA,
                                  size.width(), size.height(), 1, 0, GL_RGBA,
                                  GL_UNSIGNED_BYTE, gfx::Rect());

    static_cast<AVDACodecImage*>(image.get())
        ->set_texture(texture_ref->texture());
  }

  // If we're clearing the image, or setting a SurfaceTexture backed image, we
  // set the state to UNBOUND. For SurfaceTexture images, this ensures that the
  // implementation will call CopyTexImage, which is where AVDACodecImage
  // updates the SurfaceTexture to the right frame.
  auto image_state = gpu::gles2::Texture::UNBOUND;
  // For SurfaceView we set the state to BOUND because ScheduleOverlayPlane
  // requires it. If something tries to sample from this texture it won't work,
  // but there's no way to sample from a SurfaceView anyway, so it doesn't
  // matter.
  if (image && !surface_texture_)
    image_state = gpu::gles2::Texture::BOUND;
  texture_manager->SetLevelStreamTextureImage(texture_ref, GetTextureTarget(),
                                              0, image.get(), image_state,
                                              stream_texture_service_id);
}

void AVDAPictureBufferManager::UseCodecBufferForPictureBuffer(
    int32_t codec_buf_index,
    const PictureBuffer& picture_buffer) {
  // Make sure that the decoder is available.
  RETURN_IF_NULL(state_provider_->GetGlDecoder());

  // Notify the AVDACodecImage for picture_buffer that it should use the
  // decoded buffer codec_buf_index to render this frame.
  AVDACodecImage* avda_image =
      shared_state_->GetImageForPicture(picture_buffer.id());
  RETURN_IF_NULL(avda_image);

  // Note that this is not a race, since we do not re-use a PictureBuffer
  // until after the CC is done drawing it.
  pictures_out_for_display_.push_back(picture_buffer.id());
  avda_image->set_media_codec_buffer_index(codec_buf_index);
  avda_image->set_size(state_provider_->GetSize());

  MaybeRenderEarly();
}

void AVDAPictureBufferManager::AssignOnePictureBuffer(
    const PictureBuffer& picture_buffer,
    bool have_context) {
  // Attach a GLImage to each texture that will use the surface texture.
  // We use a refptr here in case SetImageForPicture fails.
  scoped_refptr<gpu::gles2::GLStreamTextureImage> gl_image =
      new AVDACodecImage(picture_buffer.id(), shared_state_, media_codec_,
                         state_provider_->GetGlDecoder());
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
    DCHECK_LE(1u, picture_buffer.service_texture_ids().size());
    glBindTexture(GL_TEXTURE_2D, picture_buffer.service_texture_ids()[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size.width(), size.height(), 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, rgba);
  }
}

void AVDAPictureBufferManager::ReleaseCodecBufferForPicture(
    const PictureBuffer& picture_buffer) {
  AVDACodecImage* avda_image =
      shared_state_->GetImageForPicture(picture_buffer.id());
  RETURN_IF_NULL(avda_image);
  avda_image->UpdateSurface(AVDACodecImage::UpdateMode::DISCARD_CODEC_BUFFER);
}

void AVDAPictureBufferManager::ReuseOnePictureBuffer(
    const PictureBuffer& picture_buffer) {
  pictures_out_for_display_.erase(
      std::remove(pictures_out_for_display_.begin(),
                  pictures_out_for_display_.end(), picture_buffer.id()),
      pictures_out_for_display_.end());

  // At this point, the CC must be done with the picture.  We can't really
  // check for that here directly.  it's guaranteed in gpu_video_decoder.cc,
  // when it waits on the sync point before releasing the mailbox.  That sync
  // point is inserted by destroying the resource in VideoLayerImpl::DidDraw.
  ReleaseCodecBufferForPicture(picture_buffer);
  MaybeRenderEarly();
}

void AVDAPictureBufferManager::ReleaseCodecBuffers(
    const PictureBufferMap& buffers) {
  for (const std::pair<int, PictureBuffer>& entry : buffers)
    ReleaseCodecBufferForPicture(entry.second);
}

void AVDAPictureBufferManager::MaybeRenderEarly() {
  if (pictures_out_for_display_.empty())
    return;

  // See if we can consume the front buffer / render to the SurfaceView. Iterate
  // in reverse to find the most recent front buffer. If none is found, the
  // |front_index| will point to the beginning of the array.
  size_t front_index = pictures_out_for_display_.size() - 1;
  AVDACodecImage* first_renderable_image = nullptr;
  for (int i = front_index; i >= 0; --i) {
    const int id = pictures_out_for_display_[i];
    AVDACodecImage* avda_image = shared_state_->GetImageForPicture(id);
    if (!avda_image)
      continue;

    // Update the front buffer index as we move along to shorten the number of
    // candidate images we look at for back buffer rendering.
    front_index = i;
    first_renderable_image = avda_image;

    // If we find a front buffer, stop and indicate that front buffer rendering
    // is not possible since another image is already in the front buffer.
    if (avda_image->was_rendered_to_front_buffer()) {
      first_renderable_image = nullptr;
      break;
    }
  }

  if (first_renderable_image) {
    first_renderable_image->UpdateSurface(
        AVDACodecImage::UpdateMode::RENDER_TO_FRONT_BUFFER);
  }

  // Back buffer rendering is only available for surface textures. We'll always
  // have at least one front buffer, so the next buffer must be the backbuffer.
  size_t backbuffer_index = front_index + 1;
  if (!surface_texture_ || backbuffer_index >= pictures_out_for_display_.size())
    return;

  // See if the back buffer is free. If so, then render the frame adjacent to
  // the front buffer.  The listing is in render order, so we can just use the
  // first unrendered frame if there is back buffer space.
  first_renderable_image = shared_state_->GetImageForPicture(
      pictures_out_for_display_[backbuffer_index]);
  if (!first_renderable_image ||
      first_renderable_image->was_rendered_to_back_buffer()) {
    return;
  }

  // Due to the loop in the beginning this should never be true.
  DCHECK(!first_renderable_image->was_rendered_to_front_buffer());
  first_renderable_image->UpdateSurface(
      AVDACodecImage::UpdateMode::RENDER_TO_BACK_BUFFER);
}

void AVDAPictureBufferManager::CodecChanged(VideoCodecBridge* codec) {
  media_codec_ = codec;
  shared_state_->CodecChanged(codec);
}

bool AVDAPictureBufferManager::ArePicturesOverlayable() {
  // SurfaceView frames are always overlayable because that's the only way to
  // display them.
  return !surface_texture_;
}

}  // namespace media
