// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/client/raster_implementation_gles.h"

#include "base/logging.h"
#include "cc/paint/decode_stashing_image_provider.h"
#include "cc/paint/display_item_list.h"  // nogncheck
#include "cc/paint/paint_op_buffer_serializer.h"
#include "cc/paint/transfer_cache_entry.h"
#include "cc/paint/transfer_cache_serialize_helper.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/skia_util.h"

namespace gpu {
namespace raster {

namespace {

class TransferCacheSerializeHelperImpl
    : public cc::TransferCacheSerializeHelper {
 public:
  TransferCacheSerializeHelperImpl(ContextSupport* support)
      : support_(support) {}
  ~TransferCacheSerializeHelperImpl() final = default;

 private:
  bool LockEntryInternal(cc::TransferCacheEntryType type, uint32_t id) final {
    return support_->ThreadsafeLockTransferCacheEntry(type, id);
  }

  void CreateEntryInternal(const cc::ClientTransferCacheEntry& entry) final {
    support_->CreateTransferCacheEntry(entry);
  }

  void FlushEntriesInternal(
      const std::vector<std::pair<cc::TransferCacheEntryType, uint32_t>>&
          entries) final {
    support_->UnlockTransferCacheEntries(entries);
  }

  ContextSupport* support_;
};

struct PaintOpSerializer {
 public:
  PaintOpSerializer(size_t initial_size,
                    gles2::GLES2Interface* gl,
                    cc::DecodeStashingImageProvider* stashing_image_provider,
                    cc::TransferCacheSerializeHelper* transfer_cache_helper)
      : gl_(gl),
        buffer_(static_cast<char*>(gl_->MapRasterCHROMIUM(initial_size))),
        stashing_image_provider_(stashing_image_provider),
        transfer_cache_helper_(transfer_cache_helper),
        free_bytes_(buffer_ ? initial_size : 0) {}

  ~PaintOpSerializer() {
    // Need to call SendSerializedData;
    DCHECK(!written_bytes_);
  }

  size_t Serialize(const cc::PaintOp* op,
                   const cc::PaintOp::SerializeOptions& options) {
    if (!valid())
      return 0;
    size_t size = op->Serialize(buffer_ + written_bytes_, free_bytes_, options);
    if (!size) {
      SendSerializedData();
      buffer_ = static_cast<char*>(gl_->MapRasterCHROMIUM(kBlockAlloc));
      if (!buffer_) {
        free_bytes_ = 0;
        return 0;
      }
      free_bytes_ = kBlockAlloc;
      size = op->Serialize(buffer_ + written_bytes_, free_bytes_, options);
    }
    DCHECK_LE(size, free_bytes_);

    written_bytes_ += size;
    free_bytes_ -= size;
    return size;
  }

  void SendSerializedData() {
    if (!valid())
      return;

    gl_->UnmapRasterCHROMIUM(written_bytes_);
    // Now that we've issued the RasterCHROMIUM referencing the stashed
    // images, Reset the |stashing_image_provider_|, causing us to issue
    // unlock commands for these images.
    stashing_image_provider_->Reset();
    transfer_cache_helper_->FlushEntries();
    written_bytes_ = 0;
  }

  bool valid() const { return !!buffer_; }

 private:
  static constexpr unsigned int kBlockAlloc = 512 * 1024;

  gles2::GLES2Interface* gl_;
  char* buffer_;
  cc::DecodeStashingImageProvider* stashing_image_provider_;
  cc::TransferCacheSerializeHelper* transfer_cache_helper_;

  size_t written_bytes_ = 0;
  size_t free_bytes_ = 0;
};

}  // anonymous namespace

RasterImplementationGLES::RasterImplementationGLES(
    gles2::GLES2Interface* gl,
    ContextSupport* support,
    const gpu::Capabilities& caps)
    : gl_(gl),
      support_(support),
      use_texture_storage_(caps.texture_storage),
      use_texture_storage_image_(caps.texture_storage_image) {}

RasterImplementationGLES::~RasterImplementationGLES() {}

void RasterImplementationGLES::Finish() {
  gl_->Finish();
}

void RasterImplementationGLES::Flush() {
  gl_->Flush();
}

void RasterImplementationGLES::ShallowFlushCHROMIUM() {
  gl_->ShallowFlushCHROMIUM();
}

void RasterImplementationGLES::OrderingBarrierCHROMIUM() {
  gl_->OrderingBarrierCHROMIUM();
}

void RasterImplementationGLES::GenSyncTokenCHROMIUM(GLbyte* sync_token) {
  gl_->GenSyncTokenCHROMIUM(sync_token);
}

void RasterImplementationGLES::GenUnverifiedSyncTokenCHROMIUM(
    GLbyte* sync_token) {
  gl_->GenUnverifiedSyncTokenCHROMIUM(sync_token);
}

void RasterImplementationGLES::VerifySyncTokensCHROMIUM(GLbyte** sync_tokens,
                                                        GLsizei count) {
  gl_->VerifySyncTokensCHROMIUM(sync_tokens, count);
}

void RasterImplementationGLES::WaitSyncTokenCHROMIUM(const GLbyte* sync_token) {
  gl_->WaitSyncTokenCHROMIUM(sync_token);
}

GLenum RasterImplementationGLES::GetError() {
  return gl_->GetError();
}

GLenum RasterImplementationGLES::GetGraphicsResetStatusKHR() {
  return gl_->GetGraphicsResetStatusKHR();
}

void RasterImplementationGLES::GetIntegerv(GLenum pname, GLint* params) {
  gl_->GetIntegerv(pname, params);
}

void RasterImplementationGLES::LoseContextCHROMIUM(GLenum current,
                                                   GLenum other) {
  gl_->LoseContextCHROMIUM(current, other);
}

void RasterImplementationGLES::GenQueriesEXT(GLsizei n, GLuint* queries) {
  gl_->GenQueriesEXT(n, queries);
}

void RasterImplementationGLES::DeleteQueriesEXT(GLsizei n,
                                                const GLuint* queries) {
  gl_->DeleteQueriesEXT(n, queries);
}

void RasterImplementationGLES::BeginQueryEXT(GLenum target, GLuint id) {
  gl_->BeginQueryEXT(target, id);
}

void RasterImplementationGLES::EndQueryEXT(GLenum target) {
  gl_->EndQueryEXT(target);
}

void RasterImplementationGLES::GetQueryObjectuivEXT(GLuint id,
                                                    GLenum pname,
                                                    GLuint* params) {
  gl_->GetQueryObjectuivEXT(id, pname, params);
}

void RasterImplementationGLES::GenTextures(GLsizei n, GLuint* textures) {
  gl_->GenTextures(n, textures);
}

void RasterImplementationGLES::DeleteTextures(GLsizei n,
                                              const GLuint* textures) {
  gl_->DeleteTextures(n, textures);
};

void RasterImplementationGLES::BindTexture(GLenum target, GLuint texture) {
  gl_->BindTexture(target, texture);
};

void RasterImplementationGLES::ActiveTexture(GLenum texture) {
  gl_->ActiveTexture(texture);
}

void RasterImplementationGLES::GenerateMipmap(GLenum target) {
  gl_->GenerateMipmap(target);
}

void RasterImplementationGLES::SetColorSpaceMetadataCHROMIUM(
    GLuint texture_id,
    GLColorSpace color_space) {
  gl_->SetColorSpaceMetadataCHROMIUM(texture_id, color_space);
}

void RasterImplementationGLES::TexParameteri(GLenum target,
                                             GLenum pname,
                                             GLint param) {
  gl_->TexParameteri(target, pname, param);
}

void RasterImplementationGLES::GenMailboxCHROMIUM(GLbyte* mailbox) {
  gl_->GenMailboxCHROMIUM(mailbox);
}

void RasterImplementationGLES::ProduceTextureDirectCHROMIUM(
    GLuint texture,
    const GLbyte* mailbox) {
  gl_->ProduceTextureDirectCHROMIUM(texture, mailbox);
}

GLuint RasterImplementationGLES::CreateAndConsumeTextureCHROMIUM(
    const GLbyte* mailbox) {
  return gl_->CreateAndConsumeTextureCHROMIUM(mailbox);
}

GLuint RasterImplementationGLES::CreateImageCHROMIUM(ClientBuffer buffer,
                                                     GLsizei width,
                                                     GLsizei height,
                                                     GLenum internalformat) {
  return gl_->CreateImageCHROMIUM(buffer, width, height, internalformat);
}

void RasterImplementationGLES::BindTexImage2DCHROMIUM(GLenum target,
                                                      GLint imageId) {
  gl_->BindTexImage2DCHROMIUM(target, imageId);
}

void RasterImplementationGLES::ReleaseTexImage2DCHROMIUM(GLenum target,
                                                         GLint imageId) {
  gl_->ReleaseTexImage2DCHROMIUM(target, imageId);
}

void RasterImplementationGLES::DestroyImageCHROMIUM(GLuint image_id) {
  gl_->DestroyImageCHROMIUM(image_id);
}

void RasterImplementationGLES::TexImage2D(GLenum target,
                                          GLint level,
                                          GLint internalformat,
                                          GLsizei width,
                                          GLsizei height,
                                          GLint border,
                                          GLenum format,
                                          GLenum type,
                                          const void* pixels) {
  gl_->TexImage2D(target, level, internalformat, width, height, border, format,
                  type, pixels);
}

void RasterImplementationGLES::TexSubImage2D(GLenum target,
                                             GLint level,
                                             GLint xoffset,
                                             GLint yoffset,
                                             GLsizei width,
                                             GLsizei height,
                                             GLenum format,
                                             GLenum type,
                                             const void* pixels) {
  gl_->TexSubImage2D(target, level, xoffset, yoffset, width, height, format,
                     type, pixels);
}

void RasterImplementationGLES::CompressedTexImage2D(GLenum target,
                                                    GLint level,
                                                    GLenum internalformat,
                                                    GLsizei width,
                                                    GLsizei height,
                                                    GLint border,
                                                    GLsizei imageSize,
                                                    const void* data) {
  gl_->CompressedTexImage2D(target, level, internalformat, width, height,
                            border, imageSize, data);
}

void RasterImplementationGLES::TexStorageForRaster(
    GLenum target,
    viz::ResourceFormat format,
    GLsizei width,
    GLsizei height,
    RasterTexStorageFlags flags) {
  if (flags & kOverlay) {
    DCHECK(use_texture_storage_image_);
    gl_->TexStorage2DImageCHROMIUM(target, viz::TextureStorageFormat(format),
                                   GL_SCANOUT_CHROMIUM, width, height);
  } else if (use_texture_storage_) {
    GLint levels = 1;
    gl_->TexStorage2DEXT(target, levels, viz::TextureStorageFormat(format),
                         width, height);
  } else {
    gl_->TexImage2D(target, 0, viz::GLInternalFormat(format), width, height, 0,
                    viz::GLDataFormat(format), viz::GLDataType(format),
                    nullptr);
  }
}

void RasterImplementationGLES::CopySubTextureCHROMIUM(
    GLuint source_id,
    GLint source_level,
    GLenum dest_target,
    GLuint dest_id,
    GLint dest_level,
    GLint xoffset,
    GLint yoffset,
    GLint x,
    GLint y,
    GLsizei width,
    GLsizei height,
    GLboolean unpack_flip_y,
    GLboolean unpack_premultiply_alpha,
    GLboolean unpack_unmultiply_alpha) {
  gl_->CopySubTextureCHROMIUM(source_id, source_level, dest_target, dest_id,
                              dest_level, xoffset, yoffset, x, y, width, height,
                              unpack_flip_y, unpack_premultiply_alpha,
                              unpack_unmultiply_alpha);
}

void RasterImplementationGLES::CompressedCopyTextureCHROMIUM(GLuint source_id,
                                                             GLuint dest_id) {
  gl_->CompressedCopyTextureCHROMIUM(source_id, dest_id);
}

void RasterImplementationGLES::InitializeDiscardableTextureCHROMIUM(
    GLuint texture_id) {
  gl_->InitializeDiscardableTextureCHROMIUM(texture_id);
}

void RasterImplementationGLES::UnlockDiscardableTextureCHROMIUM(
    GLuint texture_id) {
  gl_->UnlockDiscardableTextureCHROMIUM(texture_id);
}

bool RasterImplementationGLES::LockDiscardableTextureCHROMIUM(
    GLuint texture_id) {
  return gl_->LockDiscardableTextureCHROMIUM(texture_id);
}

void RasterImplementationGLES::BeginRasterCHROMIUM(
    GLuint texture_id,
    GLuint sk_color,
    GLuint msaa_sample_count,
    GLboolean can_use_lcd_text,
    GLboolean use_distance_field_text,
    GLint pixel_config) {
  gl_->BeginRasterCHROMIUM(texture_id, sk_color, msaa_sample_count,
                           can_use_lcd_text, use_distance_field_text,
                           pixel_config);
};

void RasterImplementationGLES::RasterCHROMIUM(
    const cc::DisplayItemList* list,
    cc::ImageProvider* provider,
    const gfx::Vector2d& translate,
    const gfx::Rect& playback_rect,
    const gfx::Vector2dF& post_translate,
    GLfloat post_scale) {
  if (std::abs(post_scale) < std::numeric_limits<float>::epsilon())
    return;

  gfx::Rect query_rect =
      gfx::ScaleToEnclosingRect(playback_rect, 1.f / post_scale);
  std::vector<size_t> offsets = list->rtree_.Search(query_rect);
  if (offsets.empty())
    return;

  // TODO(enne): tune these numbers
  // TODO(enne): convert these types here and in transfer buffer to be size_t.
  static constexpr unsigned int kMinAlloc = 16 * 1024;
  unsigned int free_size =
      std::max(support_->GetTransferBufferFreeSize(), kMinAlloc);

  // This section duplicates RasterSource::PlaybackToCanvas setup preamble.
  cc::PaintOpBufferSerializer::Preamble preamble;
  preamble.translation = translate;
  preamble.playback_rect = playback_rect;
  preamble.post_translation = post_translate;
  preamble.post_scale = post_scale;

  // Wrap the provided provider in a stashing provider so that we can delay
  // unrefing images until we have serialized dependent commands.
  provider->BeginRaster();
  cc::DecodeStashingImageProvider stashing_image_provider(provider);

  // TODO(enne): need to implement alpha folding optimization from POB.
  // TODO(enne): don't access private members of DisplayItemList.
  TransferCacheSerializeHelperImpl transfer_cache_serialize_helper(support_);
  PaintOpSerializer op_serializer(free_size, gl_, &stashing_image_provider,
                                  &transfer_cache_serialize_helper);
  cc::PaintOpBufferSerializer::SerializeCallback serialize_cb =
      base::BindRepeating(&PaintOpSerializer::Serialize,
                          base::Unretained(&op_serializer));
  cc::PaintOpBufferSerializer serializer(serialize_cb, &stashing_image_provider,
                                         &transfer_cache_serialize_helper);
  serializer.Serialize(&list->paint_op_buffer_, &offsets, preamble);
  // TODO(piman): raise error if !serializer.valid()?
  op_serializer.SendSerializedData();
  provider->EndRaster();
}

void RasterImplementationGLES::EndRasterCHROMIUM() {
  gl_->EndRasterCHROMIUM();
}

void RasterImplementationGLES::BeginGpuRaster() {
  // TODO(alokp): Use a trace macro to push/pop markers.
  // Using push/pop functions directly incurs cost to evaluate function
  // arguments even when tracing is disabled.
  gl_->TraceBeginCHROMIUM("BeginGpuRaster", "GpuRasterization");
}

void RasterImplementationGLES::EndGpuRaster() {
  // Restore default GL unpack alignment.  TextureUploader expects this.
  gl_->PixelStorei(GL_UNPACK_ALIGNMENT, 4);

  // TODO(alokp): Use a trace macro to push/pop markers.
  // Using push/pop functions directly incurs cost to evaluate function
  // arguments even when tracing is disabled.
  gl_->TraceEndCHROMIUM();
}

}  // namespace raster
}  // namespace gpu
