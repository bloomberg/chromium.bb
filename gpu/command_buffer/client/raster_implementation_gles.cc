// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/client/raster_implementation_gles.h"

#include "base/logging.h"
#include "cc/paint/color_space_transfer_cache_entry.h"
#include "cc/paint/decode_stashing_image_provider.h"
#include "cc/paint/display_item_list.h"  // nogncheck
#include "cc/paint/paint_op_buffer_serializer.h"
#include "cc/paint/transfer_cache_entry.h"
#include "cc/paint/transfer_cache_serialize_helper.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"
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
  bool LockEntryInternal(const EntryKey& key) final {
    return support_->ThreadsafeLockTransferCacheEntry(
        static_cast<uint32_t>(key.first), key.second);
  }

  void CreateEntryInternal(const cc::ClientTransferCacheEntry& entry) final {
    size_t size = entry.SerializedSize();
    void* data = support_->MapTransferCacheEntry(size);
    // TODO(piman): handle error (failed to allocate/map shm)
    DCHECK(data);
    bool succeeded = entry.Serialize(
        base::make_span(reinterpret_cast<uint8_t*>(data), size));
    DCHECK(succeeded);
    support_->UnmapAndCreateTransferCacheEntry(entry.UnsafeType(), entry.Id());
  }

  void FlushEntriesInternal(std::set<EntryKey> entries) final {
    std::vector<std::pair<uint32_t, uint32_t>> transformed;
    transformed.reserve(entries.size());
    for (const auto& e : entries)
      transformed.emplace_back(static_cast<uint32_t>(e.first), e.second);
    support_->UnlockTransferCacheEntries(transformed);
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

static GLenum GetImageTextureTarget(const gpu::Capabilities& caps,
                                    gfx::BufferUsage usage,
                                    viz::ResourceFormat format) {
  gfx::BufferFormat buffer_format = viz::BufferFormat(format);
  return GetBufferTextureTarget(usage, buffer_format, caps);
}

RasterImplementationGLES::Texture::Texture(GLuint id,
                                           GLenum target,
                                           bool use_buffer,
                                           gfx::BufferUsage buffer_usage,
                                           viz::ResourceFormat format)
    : id(id),
      target(target),
      use_buffer(use_buffer),
      buffer_usage(buffer_usage),
      format(format) {}

RasterImplementationGLES::Texture* RasterImplementationGLES::GetTexture(
    GLuint texture_id) {
  auto it = texture_info_.find(texture_id);
  DCHECK(it != texture_info_.end()) << "Undefined texture id";
  return &it->second;
}

RasterImplementationGLES::Texture* RasterImplementationGLES::EnsureTextureBound(
    RasterImplementationGLES::Texture* texture) {
  DCHECK(texture);
  if (bound_texture_ != texture) {
    bound_texture_ = texture;
    gl_->BindTexture(texture->target, texture->id);
  }
  return texture;
}

RasterImplementationGLES::RasterImplementationGLES(
    gles2::GLES2Interface* gl,
    ContextSupport* support,
    const gpu::Capabilities& caps)
    : gl_(gl),
      support_(support),
      caps_(caps),
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

GLuint RasterImplementationGLES::CreateTexture(bool use_buffer,
                                               gfx::BufferUsage buffer_usage,
                                               viz::ResourceFormat format) {
  GLuint texture_id = 0;
  gl_->GenTextures(1, &texture_id);
  DCHECK(texture_id);
  GLenum target = use_buffer
                      ? GetImageTextureTarget(caps_, buffer_usage, format)
                      : GL_TEXTURE_2D;
  texture_info_.emplace(std::make_pair(
      texture_id,
      Texture(texture_id, target, use_buffer, buffer_usage, format)));
  return texture_id;
}

void RasterImplementationGLES::DeleteTextures(GLsizei n,
                                              const GLuint* textures) {
  DCHECK(n > 0);
  for (GLsizei i = 0; i < n; i++) {
    auto texture_iter = texture_info_.find(textures[i]);
    DCHECK(texture_iter != texture_info_.end());

    if (bound_texture_ == &texture_iter->second)
      bound_texture_ = nullptr;

    texture_info_.erase(texture_iter);
  }

  gl_->DeleteTextures(n, textures);
};

void RasterImplementationGLES::SetColorSpaceMetadata(GLuint texture_id,
                                                     GLColorSpace color_space) {
  Texture* texture = GetTexture(texture_id);
  gl_->SetColorSpaceMetadataCHROMIUM(texture->id, color_space);
}

void RasterImplementationGLES::TexParameteri(GLuint texture_id,
                                             GLenum pname,
                                             GLint param) {
  Texture* texture = EnsureTextureBound(GetTexture(texture_id));
  gl_->TexParameteri(texture->target, pname, param);
}

void RasterImplementationGLES::GenMailbox(GLbyte* mailbox) {
  gl_->GenMailboxCHROMIUM(mailbox);
}

void RasterImplementationGLES::ProduceTextureDirect(GLuint texture_id,
                                                    const GLbyte* mailbox) {
  Texture* texture = GetTexture(texture_id);
  gl_->ProduceTextureDirectCHROMIUM(texture->id, mailbox);
}

GLuint RasterImplementationGLES::CreateAndConsumeTexture(
    bool use_buffer,
    gfx::BufferUsage buffer_usage,
    viz::ResourceFormat format,
    const GLbyte* mailbox) {
  GLuint texture_id = gl_->CreateAndConsumeTextureCHROMIUM(mailbox);
  DCHECK(texture_id);

  GLenum target = use_buffer
                      ? GetImageTextureTarget(caps_, buffer_usage, format)
                      : GL_TEXTURE_2D;
  texture_info_.emplace(std::make_pair(
      texture_id,
      Texture(texture_id, target, use_buffer, buffer_usage, format)));

  return texture_id;
}

GLuint RasterImplementationGLES::CreateImageCHROMIUM(ClientBuffer buffer,
                                                     GLsizei width,
                                                     GLsizei height,
                                                     GLenum internalformat) {
  return gl_->CreateImageCHROMIUM(buffer, width, height, internalformat);
}

void RasterImplementationGLES::BindTexImage2DCHROMIUM(GLuint texture_id,
                                                      GLint image_id) {
  Texture* texture = EnsureTextureBound(GetTexture(texture_id));
  gl_->BindTexImage2DCHROMIUM(texture->target, image_id);
}

void RasterImplementationGLES::ReleaseTexImage2DCHROMIUM(GLuint texture_id,
                                                         GLint image_id) {
  Texture* texture = EnsureTextureBound(GetTexture(texture_id));
  gl_->ReleaseTexImage2DCHROMIUM(texture->target, image_id);
}

void RasterImplementationGLES::DestroyImageCHROMIUM(GLuint image_id) {
  gl_->DestroyImageCHROMIUM(image_id);
}

void RasterImplementationGLES::TexStorage2D(GLuint texture_id,
                                            GLsizei levels,
                                            GLsizei width,
                                            GLsizei height) {
  Texture* texture = EnsureTextureBound(GetTexture(texture_id));

  if (texture->use_buffer) {
    DCHECK(use_texture_storage_image_);
    DCHECK(levels == 1);
    gl_->TexStorage2DImageCHROMIUM(texture->target,
                                   viz::TextureStorageFormat(texture->format),
                                   GL_SCANOUT_CHROMIUM, width, height);
  } else if (use_texture_storage_) {
    gl_->TexStorage2DEXT(texture->target, levels,
                         viz::TextureStorageFormat(texture->format), width,
                         height);
  } else {
    DCHECK(levels == 1);
    // TODO(vmiura): Support more than one texture level.
    gl_->TexImage2D(texture->target, 0, viz::GLInternalFormat(texture->format),
                    width, height, 0, viz::GLDataFormat(texture->format),
                    viz::GLDataType(texture->format), nullptr);
  }
}

void RasterImplementationGLES::CopySubTexture(GLuint source_id,
                                              GLuint dest_id,
                                              GLint xoffset,
                                              GLint yoffset,
                                              GLint x,
                                              GLint y,
                                              GLsizei width,
                                              GLsizei height) {
  Texture* source = GetTexture(source_id);
  Texture* dest = GetTexture(dest_id);

  gl_->CopySubTextureCHROMIUM(source->id, 0, dest->target, dest->id, 0, xoffset,
                              yoffset, x, y, width, height, false, false,
                              false);
}

void RasterImplementationGLES::CompressedCopyTextureCHROMIUM(GLuint source_id,
                                                             GLuint dest_id) {
  Texture* source = GetTexture(source_id);
  Texture* dest = GetTexture(dest_id);

  gl_->CompressedCopyTextureCHROMIUM(source->id, dest->id);
}

void RasterImplementationGLES::UnpremultiplyAndDitherCopyCHROMIUM(
    GLuint source_id,
    GLuint dest_id,
    GLint x,
    GLint y,
    GLsizei width,
    GLsizei height) {
  Texture* source = GetTexture(source_id);
  Texture* dest = GetTexture(dest_id);

  gl_->UnpremultiplyAndDitherCopyCHROMIUM(source->id, dest->id, x, y, width,
                                          height);
}

void RasterImplementationGLES::InitializeDiscardableTextureCHROMIUM(
    GLuint texture_id) {
  Texture* texture = GetTexture(texture_id);
  gl_->InitializeDiscardableTextureCHROMIUM(texture->id);
}

void RasterImplementationGLES::UnlockDiscardableTextureCHROMIUM(
    GLuint texture_id) {
  Texture* texture = GetTexture(texture_id);
  gl_->UnlockDiscardableTextureCHROMIUM(texture->id);
}

bool RasterImplementationGLES::LockDiscardableTextureCHROMIUM(
    GLuint texture_id) {
  Texture* texture = GetTexture(texture_id);
  return gl_->LockDiscardableTextureCHROMIUM(texture->id);
}

void RasterImplementationGLES::BeginRasterCHROMIUM(
    GLuint texture_id,
    GLuint sk_color,
    GLuint msaa_sample_count,
    GLboolean can_use_lcd_text,
    GLint color_type,
    const cc::RasterColorSpace& raster_color_space) {
  TransferCacheSerializeHelperImpl transfer_cache_serialize_helper(support_);
  if (!transfer_cache_serialize_helper.LockEntry(
          cc::TransferCacheEntryType::kColorSpace,
          raster_color_space.color_space_id)) {
    transfer_cache_serialize_helper.CreateEntry(
        cc::ClientColorSpaceTransferCacheEntry(raster_color_space));
  }
  transfer_cache_serialize_helper.AssertLocked(
      cc::TransferCacheEntryType::kColorSpace,
      raster_color_space.color_space_id);

  Texture* texture = GetTexture(texture_id);
  gl_->BeginRasterCHROMIUM(texture->id, sk_color, msaa_sample_count,
                           can_use_lcd_text, color_type,
                           raster_color_space.color_space_id);
  transfer_cache_serialize_helper.FlushEntries();
  background_color_ = sk_color;
};

void RasterImplementationGLES::RasterCHROMIUM(
    const cc::DisplayItemList* list,
    cc::ImageProvider* provider,
    const gfx::Size& content_size,
    const gfx::Rect& full_raster_rect,
    const gfx::Rect& playback_rect,
    const gfx::Vector2dF& post_translate,
    GLfloat post_scale,
    bool requires_clear) {
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
  preamble.content_size = content_size;
  preamble.full_raster_rect = full_raster_rect;
  preamble.playback_rect = playback_rect;
  preamble.post_translation = post_translate;
  preamble.post_scale = gfx::SizeF(post_scale, post_scale);
  preamble.requires_clear = requires_clear;
  preamble.background_color = background_color_;

  // Wrap the provided provider in a stashing provider so that we can delay
  // unrefing images until we have serialized dependent commands.
  cc::DecodeStashingImageProvider stashing_image_provider(provider);

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
}

void RasterImplementationGLES::EndRasterCHROMIUM() {
  gl_->EndRasterCHROMIUM();
}

void RasterImplementationGLES::BeginGpuRaster() {
  // Using push/pop functions directly incurs cost to evaluate function
  // arguments even when tracing is disabled.
  gl_->TraceBeginCHROMIUM("BeginGpuRaster", "GpuRasterization");
}

void RasterImplementationGLES::EndGpuRaster() {
  // Restore default GL unpack alignment.  TextureUploader expects this.
  gl_->PixelStorei(GL_UNPACK_ALIGNMENT, 4);

  // Using push/pop functions directly incurs cost to evaluate function
  // arguments even when tracing is disabled.
  gl_->TraceEndCHROMIUM();

  // Reset cached raster state.
  bound_texture_ = nullptr;
  gl_->ActiveTexture(GL_TEXTURE0);
}

}  // namespace raster
}  // namespace gpu
