// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android/shared_image_video.h"

#include <utility>

#include "components/viz/common/resources/resource_format_utils.h"
#include "components/viz/common/resources/resource_sizes.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/abstract_texture.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image_representation.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "media/gpu/android/codec_image.h"
#include "third_party/skia/include/core/SkPromiseImageTexture.h"
#include "third_party/skia/include/gpu/GrBackendSemaphore.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"

namespace media {

SharedImageVideo::SharedImageVideo(
    const gpu::Mailbox& mailbox,
    const gfx::ColorSpace color_space,
    scoped_refptr<CodecImage> codec_image,
    std::unique_ptr<gpu::gles2::AbstractTexture> abstract_texture,
    scoped_refptr<gpu::SharedContextState> shared_context_state,
    bool is_thread_safe)
    : SharedImageBacking(
          mailbox,
          viz::RGBA_8888,
          codec_image->GetSize(),
          color_space,
          (gpu::SHARED_IMAGE_USAGE_DISPLAY | gpu::SHARED_IMAGE_USAGE_GLES2),
          viz::ResourceSizes::UncheckedSizeInBytes<size_t>(
              codec_image->GetSize(),
              viz::RGBA_8888),
          is_thread_safe),
      codec_image_(std::move(codec_image)),
      abstract_texture_(std::move(abstract_texture)),
      shared_context_state_(std::move(shared_context_state)) {
  DCHECK(codec_image_);
  DCHECK(shared_context_state_);

  // Currently this backing is not thread safe.
  DCHECK(!is_thread_safe);
  shared_context_state_->AddContextLostObserver(this);
}

SharedImageVideo::~SharedImageVideo() {
  codec_image_->ReleaseCodecBuffer();
  if (shared_context_state_)
    shared_context_state_->RemoveContextLostObserver(this);
}

bool SharedImageVideo::IsCleared() const {
  return true;
}

void SharedImageVideo::SetCleared() {}

void SharedImageVideo::Update() {}

bool SharedImageVideo::ProduceLegacyMailbox(
    gpu::MailboxManager* mailbox_manager) {
  DCHECK(abstract_texture_);
  mailbox_manager->ProduceTexture(mailbox(),
                                  abstract_texture_->GetTextureBase());
  return true;
}

void SharedImageVideo::Destroy() {}

size_t SharedImageVideo::EstimatedSizeForMemTracking() const {
  // This backing contributes to gpu memory only if its bound to the texture and
  // not when the backing is created.
  return codec_image_->was_tex_image_bound() ? estimated_size() : 0;
}

void SharedImageVideo::OnContextLost() {
  // We release codec buffers when shared image context is lost. This is because
  // texture owner's texture was created on shared context. Once shared context
  // is lost, no one should try to use that texture.
  codec_image_->ReleaseCodecBuffer();
  shared_context_state_->RemoveContextLostObserver(this);
  shared_context_state_ = nullptr;
}

// Representation of a SharedImageCodecImage as a GL Texture.
class SharedImageRepresentationGLTextureVideo
    : public gpu::SharedImageRepresentationGLTexture {
 public:
  SharedImageRepresentationGLTextureVideo(gpu::SharedImageManager* manager,
                                          SharedImageVideo* backing,
                                          gpu::MemoryTypeTracker* tracker,
                                          gpu::gles2::Texture* texture)
      : gpu::SharedImageRepresentationGLTexture(manager, backing, tracker),
        texture_(texture) {}

  gpu::gles2::Texture* GetTexture() override { return texture_; }

  bool BeginAccess(GLenum mode) override {
    auto* video_backing = static_cast<SharedImageVideo*>(backing());
    DCHECK(video_backing);
    auto* codec_image = video_backing->codec_image_.get();
    auto* texture_owner = codec_image->texture_owner().get();

    // Render the codec image.
    codec_image->RenderToFrontBuffer();

    // Bind the tex image if it's not already bound.
    if (!texture_owner->binds_texture_on_update())
      texture_owner->EnsureTexImageBound();
    return true;
  }

  void EndAccess() override {}

 private:
  gpu::gles2::Texture* texture_;

  DISALLOW_COPY_AND_ASSIGN(SharedImageRepresentationGLTextureVideo);
};

// GL backed Skia representation of SharedImageVideo.
class SharedImageRepresentationVideoSkiaGL
    : public gpu::SharedImageRepresentationSkia {
 public:
  SharedImageRepresentationVideoSkiaGL(gpu::SharedImageManager* manager,
                                       gpu::SharedImageBacking* backing,
                                       gpu::MemoryTypeTracker* tracker)
      : gpu::SharedImageRepresentationSkia(manager, backing, tracker) {}

  ~SharedImageRepresentationVideoSkiaGL() override = default;

  sk_sp<SkSurface> BeginWriteAccess(
      int final_msaa_count,
      const SkSurfaceProps& surface_props,
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores) override {
    // Writes are not intended to used for video backed representations.
    NOTIMPLEMENTED();
    return nullptr;
  }

  void EndWriteAccess(sk_sp<SkSurface> surface) override { NOTIMPLEMENTED(); }

  sk_sp<SkPromiseImageTexture> BeginReadAccess(
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores) override {
    if (!promise_texture_) {
      auto* video_backing = static_cast<SharedImageVideo*>(backing());
      DCHECK(video_backing);
      auto* codec_image = video_backing->codec_image_.get();
      auto* texture_owner = codec_image->texture_owner().get();

      // Render the codec image.
      codec_image->RenderToFrontBuffer();

      // Bind the tex image if it's not already bound.
      if (!texture_owner->binds_texture_on_update())
        texture_owner->EnsureTexImageBound();
      GrBackendTexture backend_texture;
      if (!gpu::GetGrBackendTexture(
              gl::GLContext::GetCurrent()->GetVersionInfo(),
              GL_TEXTURE_EXTERNAL_OES, size(), texture_owner->GetTextureId(),
              format(), &backend_texture)) {
        return nullptr;
      }
      promise_texture_ = SkPromiseImageTexture::Make(backend_texture);
    }
    return promise_texture_;
  }

  void EndReadAccess() override {}

 private:
  sk_sp<SkPromiseImageTexture> promise_texture_;
};

// TODO(vikassoni): Currently GLRenderer doesn't support overlays with shared
// image. Add support for overlays in GLRenderer as well as overlay
// representations of shared image.
std::unique_ptr<gpu::SharedImageRepresentationGLTexture>
SharedImageVideo::ProduceGLTexture(gpu::SharedImageManager* manager,
                                   gpu::MemoryTypeTracker* tracker) {
  // For (old) overlays, we don't have a texture owner, but overlay promotion
  // might not happen for some reasons. In that case, it will try to draw
  // which should result in no image.
  if (!codec_image_->texture_owner())
    return nullptr;
  auto* texture = gpu::gles2::Texture::CheckedCast(
      codec_image_->texture_owner()->GetTextureBase());
  DCHECK(texture);

  return std::make_unique<SharedImageRepresentationGLTextureVideo>(
      manager, this, tracker, texture);
}

// Currently SkiaRenderer doesn't support overlays.
std::unique_ptr<gpu::SharedImageRepresentationSkia>
SharedImageVideo::ProduceSkia(
    gpu::SharedImageManager* manager,
    gpu::MemoryTypeTracker* tracker,
    scoped_refptr<gpu::SharedContextState> context_state) {
  DCHECK(context_state);

  // Right now we only support GL mode.
  // TODO(vikassoni): Land follow up patches to enable vulkan mode.
  if (!context_state->GrContextIsGL()) {
    DLOG(ERROR) << "SharedImage video path not yet supported in Vulkan.";
    return nullptr;
  }

  // For (old) overlays, we don't have a texture owner, but overlay promotion
  // might not happen for some reasons. In that case, it will try to draw
  // which should result in no image.
  if (!codec_image_->texture_owner())
    return nullptr;

  // In GL mode, use the texture id of the TextureOwner.
  return std::make_unique<SharedImageRepresentationVideoSkiaGL>(manager, this,
                                                                tracker);
}

}  // namespace media
