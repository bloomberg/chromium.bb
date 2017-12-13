// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/client/raster_implementation_gles.h"

#include "base/logging.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/common/capabilities.h"

namespace gpu {
namespace raster {

RasterImplementationGLES::RasterImplementationGLES(
    gles2::GLES2Interface* gl,
    const gpu::Capabilities& caps)
    : gl_(gl),
      use_texture_storage_(caps.texture_storage),
      use_texture_storage_image_(caps.texture_storage_image) {}

RasterImplementationGLES::~RasterImplementationGLES() {}

void RasterImplementationGLES::Finish() {
  gl_->Finish();
}

void RasterImplementationGLES::ShallowFlushCHROMIUM() {
  gl_->ShallowFlushCHROMIUM();
}

void RasterImplementationGLES::OrderingBarrierCHROMIUM() {
  gl_->OrderingBarrierCHROMIUM();
}

GLuint64 RasterImplementationGLES::InsertFenceSyncCHROMIUM() {
  return gl_->InsertFenceSyncCHROMIUM();
}

void RasterImplementationGLES::GenSyncTokenCHROMIUM(GLuint64 fence_sync,
                                                    GLbyte* sync_token) {
  gl_->GenSyncTokenCHROMIUM(fence_sync, sync_token);
}

void RasterImplementationGLES::GenUnverifiedSyncTokenCHROMIUM(
    GLuint64 fence_sync,
    GLbyte* sync_token) {
  gl_->GenUnverifiedSyncTokenCHROMIUM(fence_sync, sync_token);
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

void RasterImplementationGLES::RasterCHROMIUM(const cc::DisplayItemList* list,
                                              GLint translate_x,
                                              GLint translate_y,
                                              GLint clip_x,
                                              GLint clip_y,
                                              GLint clip_w,
                                              GLint clip_h,
                                              GLfloat post_translate_x,
                                              GLfloat post_translate_y,
                                              GLfloat post_scale) {
  gl_->RasterCHROMIUM(list, translate_x, translate_y, clip_x, clip_y, clip_w,
                      clip_h, post_translate_x, post_translate_y, post_scale);
}

void RasterImplementationGLES::EndRasterCHROMIUM() {
  gl_->EndRasterCHROMIUM();
}

}  // namespace raster
}  // namespace gpu
