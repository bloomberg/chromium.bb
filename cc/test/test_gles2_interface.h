// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_TEST_GLES2_INTERFACE_H_
#define CC_TEST_TEST_GLES2_INTERFACE_H_

#include "gpu/command_buffer/client/gles2_interface_stub.h"

namespace cc {
class TestWebGraphicsContext3D;

class TestGLES2Interface : public gpu::gles2::GLES2InterfaceStub {
 public:
  explicit TestGLES2Interface(TestWebGraphicsContext3D* test_context);
  virtual ~TestGLES2Interface();

  virtual void GenTextures(GLsizei n, GLuint* textures) OVERRIDE;
  virtual void GenBuffers(GLsizei n, GLuint* buffers) OVERRIDE;
  virtual void GenFramebuffers(GLsizei n, GLuint* framebuffers) OVERRIDE;
  virtual void GenQueriesEXT(GLsizei n, GLuint* queries) OVERRIDE;

  virtual void DeleteTextures(GLsizei n, const GLuint* textures) OVERRIDE;
  virtual void DeleteBuffers(GLsizei n, const GLuint* buffers) OVERRIDE;
  virtual void DeleteFramebuffers(GLsizei n,
                                  const GLuint* framebuffers) OVERRIDE;
  virtual void DeleteQueriesEXT(GLsizei n, const GLuint* queries) OVERRIDE;

  virtual GLuint CreateShader(GLenum type) OVERRIDE;
  virtual GLuint CreateProgram() OVERRIDE;

  virtual void BindTexture(GLenum target, GLuint texture) OVERRIDE;

  virtual void GetIntegerv(GLenum pname, GLint* params) OVERRIDE;
  virtual void GetShaderiv(GLuint shader, GLenum pname, GLint* params) OVERRIDE;
  virtual void GetProgramiv(GLuint program,
                            GLenum pname,
                            GLint* params) OVERRIDE;
  virtual void GetShaderPrecisionFormat(GLenum shadertype,
                                        GLenum precisiontype,
                                        GLint* range,
                                        GLint* precision) OVERRIDE;
  virtual GLenum CheckFramebufferStatus(GLenum target) OVERRIDE;

  virtual void ActiveTexture(GLenum target) OVERRIDE;
  virtual void Viewport(GLint x, GLint y, GLsizei width, GLsizei height)
      OVERRIDE;
  virtual void UseProgram(GLuint program) OVERRIDE;
  virtual void Scissor(GLint x, GLint y, GLsizei width, GLsizei height)
      OVERRIDE;
  virtual void DrawElements(GLenum mode,
                            GLsizei count,
                            GLenum type,
                            const void* indices) OVERRIDE;
  virtual void ClearColor(GLclampf red,
                          GLclampf green,
                          GLclampf blue,
                          GLclampf alpha) OVERRIDE;
  virtual void ClearStencil(GLint s) OVERRIDE;
  virtual void Clear(GLbitfield mask) OVERRIDE;
  virtual void Flush() OVERRIDE;
  virtual void Finish() OVERRIDE;
  virtual void ShallowFlushCHROMIUM() OVERRIDE;
  virtual void Enable(GLenum cap) OVERRIDE;
  virtual void Disable(GLenum cap) OVERRIDE;

  virtual void BindBuffer(GLenum target, GLuint buffer) OVERRIDE;
  virtual void BindFramebuffer(GLenum target, GLuint buffer) OVERRIDE;

  virtual void TexImage2D(GLenum target,
                          GLint level,
                          GLint internalformat,
                          GLsizei width,
                          GLsizei height,
                          GLint border,
                          GLenum format,
                          GLenum type,
                          const void* pixels) OVERRIDE;
  virtual void TexSubImage2D(GLenum target,
                             GLint level,
                             GLint xoffset,
                             GLint yoffset,
                             GLsizei width,
                             GLsizei height,
                             GLenum format,
                             GLenum type,
                             const void* pixels) OVERRIDE;
  virtual void TexStorage2DEXT(GLenum target,
                               GLsizei levels,
                               GLenum internalformat,
                               GLsizei width,
                               GLsizei height) OVERRIDE;
  virtual void TexImageIOSurface2DCHROMIUM(GLenum target,
                                           GLsizei width,
                                           GLsizei height,
                                           GLuint io_surface_id,
                                           GLuint plane) OVERRIDE;
  virtual void TexParameteri(GLenum target, GLenum pname, GLint param) OVERRIDE;

  virtual void AsyncTexImage2DCHROMIUM(GLenum target,
                                       GLint level,
                                       GLenum internalformat,
                                       GLsizei width,
                                       GLsizei height,
                                       GLint border,
                                       GLenum format,
                                       GLenum type,
                                       const void* pixels) OVERRIDE;
  virtual void AsyncTexSubImage2DCHROMIUM(GLenum target,
                                          GLint level,
                                          GLint xoffset,
                                          GLint yoffset,
                                          GLsizei width,
                                          GLsizei height,
                                          GLenum format,
                                          GLenum type,
                                          const void* pixels) OVERRIDE;
  virtual void CompressedTexImage2D(GLenum target,
                                    GLint level,
                                    GLenum internalformat,
                                    GLsizei width,
                                    GLsizei height,
                                    GLint border,
                                    GLsizei image_size,
                                    const void* data) OVERRIDE;
  virtual void WaitAsyncTexImage2DCHROMIUM(GLenum target) OVERRIDE;
  virtual GLuint CreateImageCHROMIUM(GLsizei width,
                                     GLsizei height,
                                     GLenum internalformat,
                                     GLenum usage) OVERRIDE;
  virtual void DestroyImageCHROMIUM(GLuint image_id) OVERRIDE;
  virtual void* MapImageCHROMIUM(GLuint image_id) OVERRIDE;
  virtual void GetImageParameterivCHROMIUM(GLuint image_id,
                                           GLenum pname,
                                           GLint* params) OVERRIDE;
  virtual void UnmapImageCHROMIUM(GLuint image_id) OVERRIDE;
  virtual void BindTexImage2DCHROMIUM(GLenum target, GLint image_id) OVERRIDE;
  virtual void ReleaseTexImage2DCHROMIUM(GLenum target,
                                         GLint image_id) OVERRIDE;

  virtual void* MapBufferCHROMIUM(GLuint target, GLenum access) OVERRIDE;
  virtual GLboolean UnmapBufferCHROMIUM(GLuint target) OVERRIDE;
  virtual void BufferData(GLenum target,
                          GLsizeiptr size,
                          const void* data,
                          GLenum usage) OVERRIDE;

  virtual void WaitSyncPointCHROMIUM(GLuint sync_point) OVERRIDE;
  virtual GLuint InsertSyncPointCHROMIUM() OVERRIDE;

  virtual void BeginQueryEXT(GLenum target, GLuint id) OVERRIDE;
  virtual void EndQueryEXT(GLenum target) OVERRIDE;
  virtual void GetQueryObjectuivEXT(GLuint id,
                                    GLenum pname,
                                    GLuint* params) OVERRIDE;

  virtual void DiscardFramebufferEXT(GLenum target,
                                     GLsizei count,
                                     const GLenum* attachments) OVERRIDE;
  virtual void GenMailboxCHROMIUM(GLbyte* mailbox) OVERRIDE;
  virtual void ProduceTextureCHROMIUM(GLenum target,
                                      const GLbyte* mailbox) OVERRIDE;
  virtual void ProduceTextureDirectCHROMIUM(GLuint texture,
                                            GLenum target,
                                            const GLbyte* mailbox) OVERRIDE;
  virtual void ConsumeTextureCHROMIUM(GLenum target,
                                      const GLbyte* mailbox) OVERRIDE;
  virtual GLuint CreateAndConsumeTextureCHROMIUM(
      GLenum target,
      const GLbyte* mailbox) OVERRIDE;

  virtual void ResizeCHROMIUM(GLuint width,
                              GLuint height,
                              float device_scale) OVERRIDE;
  virtual void LoseContextCHROMIUM(GLenum current, GLenum other) OVERRIDE;

 private:
  TestWebGraphicsContext3D* test_context_;
};

}  // namespace cc

#endif  // CC_TEST_TEST_GLES2_INTERFACE_H_
