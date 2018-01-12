// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_CLIENT_RASTER_IMPLEMENTATION_GLES_H_
#define GPU_COMMAND_BUFFER_CLIENT_RASTER_IMPLEMENTATION_GLES_H_

#include "base/macros.h"
#include "gles2_impl_export.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "gpu/command_buffer/common/capabilities.h"

namespace gpu {

class ContextSupport;

namespace raster {

struct Capabilities;

// An implementation of RasterInterface on top of GLES2Interface.
class GLES2_IMPL_EXPORT RasterImplementationGLES : public RasterInterface {
 public:
  RasterImplementationGLES(gles2::GLES2Interface* gl,
                           ContextSupport* support,
                           const gpu::Capabilities& caps);
  ~RasterImplementationGLES() override;

  // Command buffer Flush / Finish.
  void Finish() override;
  void Flush() override;
  void ShallowFlushCHROMIUM() override;
  void OrderingBarrierCHROMIUM() override;

  // SyncTokens.
  void GenSyncTokenCHROMIUM(GLbyte* sync_token) override;
  void GenUnverifiedSyncTokenCHROMIUM(GLbyte* sync_token) override;
  void VerifySyncTokensCHROMIUM(GLbyte** sync_tokens, GLsizei count) override;
  void WaitSyncTokenCHROMIUM(const GLbyte* sync_token) override;

  // Command buffer state.
  GLenum GetError() override;
  GLenum GetGraphicsResetStatusKHR() override;
  void GetIntegerv(GLenum pname, GLint* params) override;
  void LoseContextCHROMIUM(GLenum current, GLenum other) override;

  // Queries: GL_COMMANDS_ISSUED_CHROMIUM / GL_COMMANDS_COMPLETED_CHROMIUM.
  void GenQueriesEXT(GLsizei n, GLuint* queries) override;
  void DeleteQueriesEXT(GLsizei n, const GLuint* queries) override;
  void BeginQueryEXT(GLenum target, GLuint id) override;
  void EndQueryEXT(GLenum target) override;
  void GetQueryObjectuivEXT(GLuint id, GLenum pname, GLuint* params) override;

  // Texture objects.
  void GenTextures(GLsizei n, GLuint* textures) override;
  void DeleteTextures(GLsizei n, const GLuint* textures) override;
  void BindTexture(GLenum target, GLuint texture) override;
  void ActiveTexture(GLenum texture) override;
  void GenerateMipmap(GLenum target) override;
  void SetColorSpaceMetadataCHROMIUM(GLuint texture_id,
                                     GLColorSpace color_space) override;
  void TexParameteri(GLenum target, GLenum pname, GLint param) override;

  // Mailboxes.
  void GenMailboxCHROMIUM(GLbyte* mailbox) override;
  void ProduceTextureDirectCHROMIUM(GLuint texture,
                                    const GLbyte* mailbox) override;
  GLuint CreateAndConsumeTextureCHROMIUM(const GLbyte* mailbox) override;

  // Image objects.
  GLuint CreateImageCHROMIUM(ClientBuffer buffer,
                             GLsizei width,
                             GLsizei height,
                             GLenum internalformat) override;
  void BindTexImage2DCHROMIUM(GLenum target, GLint imageId) override;
  void ReleaseTexImage2DCHROMIUM(GLenum target, GLint imageId) override;
  void DestroyImageCHROMIUM(GLuint image_id) override;

  // Texture allocation and copying.
  void TexImage2D(GLenum target,
                  GLint level,
                  GLint internalformat,
                  GLsizei width,
                  GLsizei height,
                  GLint border,
                  GLenum format,
                  GLenum type,
                  const void* pixels) override;
  void TexSubImage2D(GLenum target,
                     GLint level,
                     GLint xoffset,
                     GLint yoffset,
                     GLsizei width,
                     GLsizei height,
                     GLenum format,
                     GLenum type,
                     const void* pixels) override;
  void CompressedTexImage2D(GLenum target,
                            GLint level,
                            GLenum internalformat,
                            GLsizei width,
                            GLsizei height,
                            GLint border,
                            GLsizei imageSize,
                            const void* data) override;
  void TexStorageForRaster(GLenum target,
                           viz::ResourceFormat format,
                           GLsizei width,
                           GLsizei height,
                           RasterTexStorageFlags flags) override;

  void CopySubTextureCHROMIUM(GLuint source_id,
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
                              GLboolean unpack_unmultiply_alpha) override;
  void CompressedCopyTextureCHROMIUM(GLuint source_id, GLuint dest_id) override;

  // Discardable textures.
  void InitializeDiscardableTextureCHROMIUM(GLuint texture_id) override;
  void UnlockDiscardableTextureCHROMIUM(GLuint texture_id) override;
  bool LockDiscardableTextureCHROMIUM(GLuint texture_id) override;

  // OOP-Raster
  void BeginRasterCHROMIUM(GLuint texture_id,
                           GLuint sk_color,
                           GLuint msaa_sample_count,
                           GLboolean can_use_lcd_text,
                           GLboolean use_distance_field_text,
                           GLint pixel_config) override;
  void RasterCHROMIUM(const cc::DisplayItemList* list,
                      cc::ImageProvider* provider,
                      const gfx::Vector2d& translate,
                      const gfx::Rect& playback_rect,
                      const gfx::Vector2dF& post_translate,
                      GLfloat post_scale) override;
  void EndRasterCHROMIUM() override;

  // Raster via GrContext.
  void BeginGpuRaster() override;
  void EndGpuRaster() override;

 private:
  gles2::GLES2Interface* gl_;
  ContextSupport* support_;
  bool use_texture_storage_;
  bool use_texture_storage_image_;

  DISALLOW_COPY_AND_ASSIGN(RasterImplementationGLES);
};

}  // namespace raster
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_RASTER_IMPLEMENTATION_GLES_H_
