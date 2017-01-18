// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_COPY_TEXTURE_CHROMIUM_H_
#define GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_COPY_TEXTURE_CHROMIUM_H_

#include <vector>

#include "base/containers/hash_tables.h"
#include "base/macros.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/command_buffer/service/gl_utils.h"
#include "gpu/gpu_export.h"

namespace gpu {
namespace gles2 {

class GLES2Decoder;

enum CopyTextureMethod {
  // Use CopyTex{Sub}Image2D to copy from the source to the destination.
  DIRECT_COPY,
  // Draw from the source to the destination texture.
  DIRECT_DRAW,
  // Draw to an intermediate texture, and then copy to the destination texture.
  DRAW_AND_COPY,
  // CopyTexture isn't available.
  NOT_COPYABLE
};

// TODOs(qiankun.miao@intel.com):
// 1. Add readback path for RGB9_E5 and float formats (if extension isn't
// available and they are not color-renderable).
// 2. Support faces of cube map texture as valid dest target. The cube map
// texture may be incomplete currently.
// 3. Add support for levels other than 0.
// 4. Support ALPHA, LUMINANCE and LUMINANCE_ALPHA formats on core profile.
// 5. Update the extension doc after the whole work is done
// in gpu/GLES2/extensions/CHROMIUM/CHROMIUM_copy_texture.txt. We probably
// will need a ES2 version and a ES3 version.

// This class encapsulates the resources required to implement the
// GL_CHROMIUM_copy_texture extension.  The copy operation is performed
// via glCopyTexImage2D() or a blit to a framebuffer object.
// The target of |dest_id| texture must be GL_TEXTURE_2D.
class GPU_EXPORT CopyTextureCHROMIUMResourceManager {
 public:
  CopyTextureCHROMIUMResourceManager();
  ~CopyTextureCHROMIUMResourceManager();

  void Initialize(const gles2::GLES2Decoder* decoder,
                  const gles2::FeatureInfo::FeatureFlags& feature_flags);
  void Destroy();

  void DoCopyTexture(const gles2::GLES2Decoder* decoder,
                     GLenum source_target,
                     GLuint source_id,
                     GLint source_level,
                     GLenum source_internal_format,
                     GLenum dest_target,
                     GLuint dest_id,
                     GLint dest_level,
                     GLenum dest_internal_format,
                     GLsizei width,
                     GLsizei height,
                     bool flip_y,
                     bool premultiply_alpha,
                     bool unpremultiply_alpha,
                     CopyTextureMethod method);

  void DoCopySubTexture(const gles2::GLES2Decoder* decoder,
                        GLenum source_target,
                        GLuint source_id,
                        GLint source_level,
                        GLenum source_internal_format,
                        GLenum dest_target,
                        GLuint dest_id,
                        GLint dest_level,
                        GLenum dest_internal_format,
                        GLint xoffset,
                        GLint yoffset,
                        GLint x,
                        GLint y,
                        GLsizei width,
                        GLsizei height,
                        GLsizei dest_width,
                        GLsizei dest_height,
                        GLsizei source_width,
                        GLsizei source_height,
                        bool flip_y,
                        bool premultiply_alpha,
                        bool unpremultiply_alpha,
                        CopyTextureMethod method);

  void DoCopySubTextureWithTransform(const gles2::GLES2Decoder* decoder,
                                     GLenum source_target,
                                     GLuint source_id,
                                     GLint source_level,
                                     GLenum source_internal_format,
                                     GLenum dest_target,
                                     GLuint dest_id,
                                     GLint dest_level,
                                     GLenum dest_internal_format,
                                     GLint xoffset,
                                     GLint yoffset,
                                     GLint x,
                                     GLint y,
                                     GLsizei width,
                                     GLsizei height,
                                     GLsizei dest_width,
                                     GLsizei dest_height,
                                     GLsizei source_width,
                                     GLsizei source_height,
                                     bool flip_y,
                                     bool premultiply_alpha,
                                     bool unpremultiply_alpha,
                                     const GLfloat transform_matrix[16]);

  // This will apply a transform on the texture coordinates before sampling
  // the source texture and copying to the destination texture. The transform
  // matrix should be given in column-major form, so it can be passed
  // directly to GL.
  void DoCopyTextureWithTransform(const gles2::GLES2Decoder* decoder,
                                  GLenum source_target,
                                  GLuint source_id,
                                  GLint source_level,
                                  GLenum source_format,
                                  GLenum dest_target,
                                  GLuint dest_id,
                                  GLint dest_level,
                                  GLenum dest_format,
                                  GLsizei width,
                                  GLsizei height,
                                  bool flip_y,
                                  bool premultiply_alpha,
                                  bool unpremultiply_alpha,
                                  const GLfloat transform_matrix[16]);

  // The attributes used during invocation of the extension.
  static const GLuint kVertexPositionAttrib = 0;

 private:
  struct ProgramInfo {
    ProgramInfo()
        : program(0u),
          vertex_dest_mult_handle(0u),
          vertex_dest_add_handle(0u),
          vertex_source_mult_handle(0u),
          vertex_source_add_handle(0u),
          tex_coord_transform_handle(0u),
          sampler_handle(0u) {}

    GLuint program;

    // Transformations that map from the original quad coordinates [-1, 1] into
    // the destination texture's quad coordinates.
    GLuint vertex_dest_mult_handle;
    GLuint vertex_dest_add_handle;

    // Transformations that map from the original quad coordinates [-1, 1] into
    // the source texture's texture coordinates.
    GLuint vertex_source_mult_handle;
    GLuint vertex_source_add_handle;

    GLuint tex_coord_transform_handle;
    GLuint sampler_handle;
  };

  void DoCopyTextureInternal(const gles2::GLES2Decoder* decoder,
                             GLenum source_target,
                             GLuint source_id,
                             GLint source_level,
                             GLenum source_format,
                             GLenum dest_target,
                             GLuint dest_id,
                             GLint dest_level,
                             GLenum dest_format,
                             GLint xoffset,
                             GLint yoffset,
                             GLint x,
                             GLint y,
                             GLsizei width,
                             GLsizei height,
                             GLsizei dest_width,
                             GLsizei dest_height,
                             GLsizei source_width,
                             GLsizei source_height,
                             bool flip_y,
                             bool premultiply_alpha,
                             bool unpremultiply_alpha,
                             const GLfloat transform_matrix[16]);

  bool initialized_;
  bool nv_egl_stream_consumer_external_;
  typedef std::vector<GLuint> ShaderVector;
  ShaderVector vertex_shaders_;
  ShaderVector fragment_shaders_;
  typedef int ProgramMapKey;
  typedef base::hash_map<ProgramMapKey, ProgramInfo> ProgramMap;
  ProgramMap programs_;
  GLuint vertex_array_object_id_;
  GLuint buffer_id_;
  GLuint framebuffer_;

  DISALLOW_COPY_AND_ASSIGN(CopyTextureCHROMIUMResourceManager);
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_COPY_TEXTURE_CHROMIUM_H_
