// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/client/raster_implementation_gles.h"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <set>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "cc/paint/color_space_transfer_cache_entry.h"
#include "cc/paint/decode_stashing_image_provider.h"
#include "cc/paint/display_item_list.h"  // nogncheck
#include "cc/paint/paint_op_buffer_serializer.h"
#include "cc/paint/transfer_cache_entry.h"
#include "cc/paint/transfer_cache_serialize_helper.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/skia_util.h"

namespace gpu {
namespace raster {

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

RasterImplementationGLES::RasterImplementationGLES(
    gles2::GLES2Interface* gl,
    const gpu::Capabilities& caps)
    : gl_(gl), caps_(caps) {}

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

void RasterImplementationGLES::DeleteTextures(GLsizei n,
                                              const GLuint* textures) {
  DCHECK_GT(n, 0);
  for (GLsizei i = 0; i < n; i++) {
    auto texture_iter = texture_info_.find(textures[i]);
    DCHECK(texture_iter != texture_info_.end());

    texture_info_.erase(texture_iter);
  }

  gl_->DeleteTextures(n, textures);
}

GLuint RasterImplementationGLES::CreateAndConsumeTexture(
    bool use_buffer,
    gfx::BufferUsage buffer_usage,
    viz::ResourceFormat format,
    const GLbyte* mailbox) {
  GLuint texture_id = gl_->CreateAndConsumeTextureCHROMIUM(mailbox);
  DCHECK(texture_id);
  DCHECK(!viz::IsResourceFormatCompressed(format));

  GLenum target = use_buffer
                      ? GetImageTextureTarget(caps_, buffer_usage, format)
                      : GL_TEXTURE_2D;
  texture_info_.emplace(std::make_pair(
      texture_id,
      Texture(texture_id, target, use_buffer, buffer_usage, format)));

  return texture_id;
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

void RasterImplementationGLES::BeginRasterCHROMIUM(
    GLuint sk_color,
    GLuint msaa_sample_count,
    GLboolean can_use_lcd_text,
    GLint color_type,
    const cc::RasterColorSpace& raster_color_space,
    const GLbyte* mailbox) {
  NOTREACHED();
}

void RasterImplementationGLES::RasterCHROMIUM(
    const cc::DisplayItemList* list,
    cc::ImageProvider* provider,
    const gfx::Size& content_size,
    const gfx::Rect& full_raster_rect,
    const gfx::Rect& playback_rect,
    const gfx::Vector2dF& post_translate,
    GLfloat post_scale,
    bool requires_clear) {
  NOTREACHED();
}

void RasterImplementationGLES::SetActiveURLCHROMIUM(const char* url) {
  gl_->SetActiveURLCHROMIUM(url);
}

void RasterImplementationGLES::EndRasterCHROMIUM() {
  NOTREACHED();
}

SyncToken RasterImplementationGLES::ScheduleImageDecode(
    base::span<const uint8_t> encoded_data,
    const gfx::Size& output_size,
    uint32_t transfer_cache_entry_id,
    const gfx::ColorSpace& target_color_space,
    bool needs_mips) {
  NOTREACHED();
  return SyncToken();
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
  gl_->ActiveTexture(GL_TEXTURE0);
}

void RasterImplementationGLES::TraceBeginCHROMIUM(const char* category_name,
                                                  const char* trace_name) {
  gl_->TraceBeginCHROMIUM(category_name, trace_name);
}

void RasterImplementationGLES::TraceEndCHROMIUM() {
  gl_->TraceEndCHROMIUM();
}

}  // namespace raster
}  // namespace gpu
