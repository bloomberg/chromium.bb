// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/buffer.h"

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES2/gl2extchromium.h>

#include <algorithm>

#include "base/logging.h"
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

gpu::gles2::GLES2Interface* GetContextGL() {
  ui::ContextFactory* context_factory =
      aura::Env::GetInstance()->context_factory();
  return context_factory->SharedMainThreadContextProvider()->ContextGL();
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// Buffer, public:

Buffer::Buffer(scoped_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer,
               unsigned texture_target)
    : gpu_memory_buffer_(gpu_memory_buffer.Pass()),
      texture_target_(texture_target),
      texture_id_(0),
      image_id_(0) {
  gpu::gles2::GLES2Interface* gles2 = GetContextGL();
  // Create an image for |gpu_memory_buffer_|.
  gfx::Size size = gpu_memory_buffer_->GetSize();
  image_id_ = gles2->CreateImageCHROMIUM(
      gpu_memory_buffer_->AsClientBuffer(), size.width(), size.height(),
      GLInternalFormat(gpu_memory_buffer_->GetFormat()));
  // Create a texture with |texture_target_|.
  gles2->ActiveTexture(GL_TEXTURE0);
  gles2->GenTextures(1, &texture_id_);
  gles2->BindTexture(texture_target_, texture_id_);
  gles2->TexParameteri(texture_target_, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  gles2->TexParameteri(texture_target_, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  gles2->TexParameteri(texture_target_, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  gles2->TexParameteri(texture_target_, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  // Generate a crypto-secure random mailbox name.
  gles2->GenMailboxCHROMIUM(mailbox_.name);
  gles2->ProduceTextureCHROMIUM(texture_target_, mailbox_.name);
}

Buffer::~Buffer() {
  gpu::gles2::GLES2Interface* gles2 = GetContextGL();
  if (texture_id_)
    gles2->DeleteTextures(1, &texture_id_);
  if (image_id_)
    gles2->DestroyImageCHROMIUM(image_id_);
}

scoped_ptr<cc::SingleReleaseCallback> Buffer::AcquireTextureMailbox(
    cc::TextureMailbox* texture_mailbox) {
  // Buffer can only be used by one client at a time. If texture id is 0, then a
  // previous call to AcquireTextureMailbox() is using this buffer and it has
  // not been released yet.
  if (!texture_id_) {
    DLOG(WARNING) << "Client tried to use a buffer that has not been released";
    return nullptr;
  }

  // Take ownerhsip of image and texture ids.
  unsigned texture_id = 0;
  unsigned image_id = 0;
  std::swap(texture_id, texture_id_);
  DCHECK_NE(image_id_, 0u);
  std::swap(image_id, image_id_);

  // Bind texture to |texture_target_|.
  gpu::gles2::GLES2Interface* gles2 = GetContextGL();
  gles2->ActiveTexture(GL_TEXTURE0);
  gles2->BindTexture(texture_target_, texture_id);

  // Bind the image to texture.
  gles2->BindTexImage2DCHROMIUM(texture_target_, image_id);

  // Create a sync token to ensure that the BindTexImage2DCHROMIUM call is
  // processed before issuing any commands that will read from texture.
  uint64 fence_sync = gles2->InsertFenceSyncCHROMIUM();
  gles2->ShallowFlushCHROMIUM();
  gpu::SyncToken sync_token;
  gles2->GenUnverifiedSyncTokenCHROMIUM(fence_sync, sync_token.GetData());

  bool is_overlay_candidate = false;
  *texture_mailbox =
      cc::TextureMailbox(mailbox_, sync_token, texture_target_,
                         gpu_memory_buffer_->GetSize(), is_overlay_candidate);
  return cc::SingleReleaseCallback::Create(
             base::Bind(&Buffer::Release, AsWeakPtr(), texture_target_,
                        texture_id, image_id))
      .Pass();
}

gfx::Size Buffer::GetSize() const {
  return gpu_memory_buffer_->GetSize();
}

scoped_refptr<base::trace_event::TracedValue> Buffer::AsTracedValue() const {
  scoped_refptr<base::trace_event::TracedValue> value =
      new base::trace_event::TracedValue;
  value->SetInteger("width", GetSize().width());
  value->SetInteger("height", GetSize().height());
  value->SetInteger("format",
                    static_cast<int>(gpu_memory_buffer_->GetFormat()));
  return value;
}

////////////////////////////////////////////////////////////////////////////////
// Buffer, private:

// static
void Buffer::Release(base::WeakPtr<Buffer> buffer,
                     unsigned texture_target,
                     unsigned texture_id,
                     unsigned image_id,
                     const gpu::SyncToken& sync_token,
                     bool is_lost) {
  TRACE_EVENT1("exo", "Buffer::Release", "is_lost", is_lost);

  gpu::gles2::GLES2Interface* gles2 = GetContextGL();
  if (sync_token.HasData())
    gles2->WaitSyncTokenCHROMIUM(sync_token.GetConstData());
  gles2->ActiveTexture(GL_TEXTURE0);
  gles2->BindTexture(texture_target, texture_id);
  gles2->ReleaseTexImage2DCHROMIUM(texture_target, image_id);

  // Delete resources and return if buffer is gone.
  if (!buffer) {
    gles2->DeleteTextures(1, &texture_id);
    gles2->DestroyImageCHROMIUM(image_id);
    return;
  }

  DCHECK_EQ(buffer->texture_id_, 0u);
  buffer->texture_id_ = texture_id;
  DCHECK_EQ(buffer->image_id_, 0u);
  buffer->image_id_ = image_id;

  if (!buffer->release_callback_.is_null())
    buffer->release_callback_.Run();
}

}  // namespace exo
