// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/buffer.h"

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES2/gl2extchromium.h>
#include <stdint.h>
#include <algorithm>
#include <utility>

#include "base/logging.h"
#include "base/macros.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_argument.h"
#include "cc/output/context_provider.h"
#include "cc/resources/single_release_callback.h"
#include "cc/resources/texture_mailbox.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "ui/aura/env.h"
#include "ui/compositor/compositor.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace exo {
namespace {

GLenum GLInternalFormat(gfx::BufferFormat format) {
  const GLenum kGLInternalFormats[] = {
      GL_ATC_RGBA_INTERPOLATED_ALPHA_AMD,  // ATC
      GL_COMPRESSED_RGB_S3TC_DXT1_EXT,     // ATCIA
      GL_COMPRESSED_RGB_S3TC_DXT1_EXT,     // DXT1
      GL_COMPRESSED_RGBA_S3TC_DXT5_EXT,    // DXT5
      GL_ETC1_RGB8_OES,                    // ETC1
      GL_R8_EXT,                           // R_8
      GL_RGBA,                             // RGBA_4444
      GL_RGB,                              // RGBX_8888
      GL_RGBA,                             // RGBA_8888
      GL_RGB,                              // BGRX_8888
      GL_BGRA_EXT,                         // BGRA_8888
      GL_RGB_YUV_420_CHROMIUM,             // YUV_420
      GL_INVALID_ENUM,                     // YUV_420_BIPLANAR
      GL_RGB_YCBCR_422_CHROMIUM,           // UYVY_422
  };
  static_assert(arraysize(kGLInternalFormats) ==
                    (static_cast<int>(gfx::BufferFormat::LAST) + 1),
                "BufferFormat::LAST must be last value of kGLInternalFormats");

  DCHECK(format <= gfx::BufferFormat::LAST);
  return kGLInternalFormats[static_cast<int>(format)];
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// Buffer::Texture

// Encapsulates the state and logic needed to bind a buffer to a GLES2 texture.
class Buffer::Texture {
 public:
  Texture(gfx::GpuMemoryBuffer* gpu_memory_buffer,
          cc::ContextProvider* context_provider,
          unsigned texture_target);
  ~Texture();

  // Returns true if GLES2 resources for texture have been lost.
  bool IsLost();

  // Binds the content of gpu memory buffer to the texture returned by
  // mailbox(). Returns a sync token that can be used to when accessing texture
  // from a different context.
  gpu::SyncToken BindTexImage();

  // Releases the content of gpu memory buffer after |sync_token| has passed.
  void ReleaseTexImage(const gpu::SyncToken& sync_token);

  // Returns the mailbox for this texture.
  gpu::Mailbox mailbox() const { return mailbox_; }

 private:
  scoped_refptr<cc::ContextProvider> context_provider_;
  const unsigned texture_target_;
  const gfx::Size size_;
  unsigned image_id_;
  unsigned texture_id_;
  gpu::Mailbox mailbox_;

  DISALLOW_COPY_AND_ASSIGN(Texture);
};

Buffer::Texture::Texture(gfx::GpuMemoryBuffer* gpu_memory_buffer,
                         cc::ContextProvider* context_provider,
                         unsigned texture_target)
    : context_provider_(context_provider),
      texture_target_(texture_target),
      size_(gpu_memory_buffer->GetSize()),
      image_id_(0),
      texture_id_(0) {
  gpu::gles2::GLES2Interface* gles2 = context_provider_->ContextGL();
  image_id_ = gles2->CreateImageCHROMIUM(
      gpu_memory_buffer->AsClientBuffer(), size_.width(), size_.height(),
      GLInternalFormat(gpu_memory_buffer->GetFormat()));
  gles2->GenTextures(1, &texture_id_);
  gles2->ActiveTexture(GL_TEXTURE0);
  gles2->BindTexture(texture_target_, texture_id_);
  gles2->TexParameteri(texture_target_, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  gles2->TexParameteri(texture_target_, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  gles2->TexParameteri(texture_target_, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  gles2->TexParameteri(texture_target_, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  // Generate a crypto-secure random mailbox name.
  gles2->GenMailboxCHROMIUM(mailbox_.name);
  gles2->ProduceTextureCHROMIUM(texture_target_, mailbox_.name);
}

Buffer::Texture::~Texture() {
  gpu::gles2::GLES2Interface* gles2 = context_provider_->ContextGL();
  gles2->ActiveTexture(GL_TEXTURE0);
  gles2->DeleteTextures(1, &texture_id_);
  gles2->DestroyImageCHROMIUM(image_id_);
}

bool Buffer::Texture::IsLost() {
  gpu::gles2::GLES2Interface* gles2 = context_provider_->ContextGL();
  return gles2->GetGraphicsResetStatusKHR() != GL_NO_ERROR;
}

gpu::SyncToken Buffer::Texture::BindTexImage() {
  gpu::gles2::GLES2Interface* gles2 = context_provider_->ContextGL();
  gles2->ActiveTexture(GL_TEXTURE0);
  gles2->BindTexture(texture_target_, texture_id_);
  gles2->BindTexImage2DCHROMIUM(texture_target_, image_id_);
  // Create and return a sync token that can be used to ensure that the
  // BindTexImage2DCHROMIUM call is processed before issuing any commands
  // that will read from the texture on a different context.
  uint64_t fence_sync = gles2->InsertFenceSyncCHROMIUM();
  gles2->OrderingBarrierCHROMIUM();
  gpu::SyncToken sync_token;
  gles2->GenUnverifiedSyncTokenCHROMIUM(fence_sync, sync_token.GetData());
  return sync_token;
}

void Buffer::Texture::ReleaseTexImage(const gpu::SyncToken& sync_token) {
  gpu::gles2::GLES2Interface* gles2 = context_provider_->ContextGL();
  if (sync_token.HasData())
    gles2->WaitSyncTokenCHROMIUM(sync_token.GetConstData());
  gles2->ActiveTexture(GL_TEXTURE0);
  gles2->BindTexture(texture_target_, texture_id_);
  gles2->ReleaseTexImage2DCHROMIUM(texture_target_, image_id_);
}

////////////////////////////////////////////////////////////////////////////////
// Buffer, public:

Buffer::Buffer(scoped_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer,
               unsigned texture_target)
    : gpu_memory_buffer_(std::move(gpu_memory_buffer)),
      texture_target_(texture_target),
      use_count_(0) {}

Buffer::~Buffer() {}

scoped_ptr<cc::SingleReleaseCallback> Buffer::ProduceTextureMailbox(
    cc::TextureMailbox* texture_mailbox) {
  DLOG_IF(WARNING, use_count_)
      << "Producing a texture mailbox for a buffer that has not been released";

  // Increment the use count for this buffer.
  ++use_count_;

  // Creating a new texture is relatively expensive so we reuse the last
  // texture whenever possible.
  scoped_ptr<Texture> texture = std::move(last_texture_);

  // If texture is lost, destroy it to ensure that we create a new one below.
  if (texture && texture->IsLost())
    texture.reset();

  // Create a new texture if one doesn't already exist. The contents of this
  // buffer can be bound to the texture using a call to BindTexImage and must
  // be released using a matching ReleaseTexImage call before it can be reused
  // or destroyed.
  if (!texture) {
    // Note: This can fail if GPU acceleration has been disabled.
    scoped_refptr<cc::ContextProvider> context_provider =
        aura::Env::GetInstance()
            ->context_factory()
            ->SharedMainThreadContextProvider();
    if (!context_provider) {
      DLOG(WARNING) << "Failed to acquire a context provider";
      Release();  // Decrements the use count
      return nullptr;
    }
    texture = make_scoped_ptr(new Texture(
        gpu_memory_buffer_.get(), context_provider.get(), texture_target_));
  }

  // This binds the latest contents of this buffer to the texture.
  gpu::SyncToken sync_token = texture->BindTexImage();

  bool is_overlay_candidate = false;
  *texture_mailbox =
      cc::TextureMailbox(texture->mailbox(), sync_token, texture_target_,
                         gpu_memory_buffer_->GetSize(), is_overlay_candidate);
  return cc::SingleReleaseCallback::Create(
      base::Bind(&Buffer::ReleaseTexture, AsWeakPtr(), base::Passed(&texture)));
}

gfx::Size Buffer::GetSize() const {
  return gpu_memory_buffer_->GetSize();
}

scoped_refptr<base::trace_event::TracedValue> Buffer::AsTracedValue() const {
  scoped_refptr<base::trace_event::TracedValue> value =
      new base::trace_event::TracedValue;
  gfx::Size size = gpu_memory_buffer_->GetSize();
  value->SetInteger("width", size.width());
  value->SetInteger("height", size.height());
  value->SetInteger("format",
                    static_cast<int>(gpu_memory_buffer_->GetFormat()));
  return value;
}

////////////////////////////////////////////////////////////////////////////////
// Buffer, private:

void Buffer::Release() {
  DCHECK_GT(use_count_, 0u);
  if (--use_count_)
    return;

  // Run release callback to notify the client that buffer has been released.
  if (!release_callback_.is_null())
    release_callback_.Run();
}

// static
void Buffer::ReleaseTexture(base::WeakPtr<Buffer> buffer,
                            scoped_ptr<Texture> texture,
                            const gpu::SyncToken& sync_token,
                            bool is_lost) {
  TRACE_EVENT1("exo", "Buffer::ReleaseTexture", "is_lost", is_lost);

  // Release image so it can safely be reused or destroyed.
  texture->ReleaseTexImage(sync_token);

  // Early out if buffer is gone. This can happen when the client destroyed the
  // buffer before receiving a release callback.
  if (!buffer)
    return;

  // Allow buffer to reused texture if it's not lost.
  if (!is_lost)
    buffer->last_texture_ = std::move(texture);

  buffer->Release();
}

}  // namespace exo
