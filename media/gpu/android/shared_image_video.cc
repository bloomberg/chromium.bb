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
#include "gpu/command_buffer/service/shared_image_representation.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "media/gpu/android/codec_image.h"

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

    // For (old) overlays, we don't have a texture owner, but overlay promotion
    // might not happen for some reasons. In that case, it will try to draw
    // which should results in no image.
    if (!texture_owner())
      return true;

    // Render the codec image.
    codec_image()->RenderToFrontBuffer();

    // Bind the tex image if its not already bound.
    if (!texture_owner()->binds_texture_on_update())
      texture_owner()->EnsureTexImageBound();
    return true;
  }

  void EndAccess() override {}

 private:
  SharedImageVideo* video_backing() {
    auto* video_backing = static_cast<SharedImageVideo*>(backing());
    return video_backing;
  }

  CodecImage* codec_image() {
    DCHECK(video_backing());
    return video_backing()->codec_image_.get();
  }

  TextureOwner* texture_owner() { return codec_image()->texture_owner().get(); }

  gpu::gles2::Texture* texture_;

  DISALLOW_COPY_AND_ASSIGN(SharedImageRepresentationGLTextureVideo);
};

std::unique_ptr<gpu::SharedImageRepresentationGLTexture>
SharedImageVideo::ProduceGLTexture(gpu::SharedImageManager* manager,
                                   gpu::MemoryTypeTracker* tracker) {
  // TODO(vikassoni): Also fix how overlays work with shared images to enable
  // this representation. To make overlays work, we need to add a new overlay
  // representation which can notify promotion hints and schedule overlay
  // planes via |codec_image_|.
  NOTIMPLEMENTED();
  return nullptr;
}

std::unique_ptr<gpu::SharedImageRepresentationSkia>
SharedImageVideo::ProduceSkia(
    gpu::SharedImageManager* manager,
    gpu::MemoryTypeTracker* tracker,
    scoped_refptr<gpu::SharedContextState> context_state) {
  // TODO(vikassoni): Implement in follow up patch.
  NOTIMPLEMENTED();
  return nullptr;
}

}  // namespace media
