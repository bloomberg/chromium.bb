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

  virtual void GenTextures(GLsizei n, GLuint* textures) override;
  virtual void GenBuffers(GLsizei n, GLuint* buffers) override;
  virtual void GenFramebuffers(GLsizei n, GLuint* framebuffers) override;
  virtual void GenRenderbuffers(GLsizei n, GLuint* renderbuffers) override;
  virtual void GenQueriesEXT(GLsizei n, GLuint* queries) override;

  virtual void DeleteTextures(GLsizei n, const GLuint* textures) override;
  virtual void DeleteBuffers(GLsizei n, const GLuint* buffers) override;
  virtual void DeleteFramebuffers(GLsizei n,
                                  const GLuint* framebuffers) override;
  virtual void DeleteQueriesEXT(GLsizei n, const GLuint* queries) override;

  virtual GLuint CreateShader(GLenum type) override;
  virtual GLuint CreateProgram() override;

  virtual void BindTexture(GLenum target, GLuint texture) override;

  virtual void GetIntegerv(GLenum pname, GLint* params) override;
  virtual void GetShaderiv(GLuint shader, GLenum pname, GLint* params) override;
  virtual void GetProgramiv(GLuint program,
                            GLenum pname,
                            GLint* params) override;
  virtual void GetShaderPrecisionFormat(GLenum shadertype,
                                        GLenum precisiontype,
                                        GLint* range,
                                        GLint* precision) override;
  virtual GLenum CheckFramebufferStatus(GLenum target) override;

  virtual void ActiveTexture(GLenum target) override;
  virtual void Viewport(GLint x, GLint y, GLsizei width, GLsizei height)
      override;
  virtual void UseProgram(GLuint program) override;
  virtual void Scissor(GLint x, GLint y, GLsizei width, GLsizei height)
      override;
  virtual void DrawElements(GLenum mode,
                            GLsizei count,
                            GLenum type,
                            const void* indices) override;
  virtual void ClearColor(GLclampf red,
                          GLclampf green,
                          GLclampf blue,
                          GLclampf alpha) override;
  virtual void ClearStencil(GLint s) override;
  virtual void Clear(GLbitfield mask) override;
  virtual void Flush() override;
  virtual void Finish() override;
  virtual void ShallowFlushCHROMIUM() override;
  virtual void Enable(GLenum cap) override;
  virtual void Disable(GLenum cap) override;

  virtual void BindBuffer(GLenum target, GLuint buffer) override;
  virtual void BindRenderbuffer(GLenum target, GLuint buffer) override;
  virtual void BindFramebuffer(GLenum target, GLuint buffer) override;

  virtual void TexImage2D(GLenum target,
                          GLint level,
                          GLint internalformat,
                          GLsizei width,
                          GLsizei height,
                          GLint border,
                          GLenum format,
                          GLenum type,
                          const void* pixels) override;
  virtual void TexSubImage2D(GLenum target,
                             GLint level,
                             GLint xoffset,
                             GLint yoffset,
                             GLsizei width,
                             GLsizei height,
                             GLenum format,
                             GLenum type,
                             const void* pixels) override;
  virtual void TexStorage2DEXT(GLenum target,
                               GLsizei levels,
                               GLenum internalformat,
                               GLsizei width,
                               GLsizei height) override;
  virtual void TexImageIOSurface2DCHROMIUM(GLenum target,
                                           GLsizei width,
                                           GLsizei height,
                                           GLuint io_surface_id,
                                           GLuint plane) override;
  virtual void TexParameteri(GLenum target, GLenum pname, GLint param) override;

  virtual void AsyncTexImage2DCHROMIUM(GLenum target,
                                       GLint level,
                                       GLenum internalformat,
                                       GLsizei width,
                                       GLsizei height,
                                       GLint border,
                                       GLenum format,
                                       GLenum type,
                                       const void* pixels) override;
  virtual void AsyncTexSubImage2DCHROMIUM(GLenum target,
                                          GLint level,
                                          GLint xoffset,
                                          GLint yoffset,
                                          GLsizei width,
                                          GLsizei height,
                                          GLenum format,
                                          GLenum type,
                                          const void* pixels) override;
  virtual void CompressedTexImage2D(GLenum target,
                                    GLint level,
                                    GLenum internalformat,
                                    GLsizei width,
                                    GLsizei height,
                                    GLint border,
                                    GLsizei image_size,
                                    const void* data) override;
  virtual void WaitAsyncTexImage2DCHROMIUM(GLenum target) override;
  virtual GLuint CreateImageCHROMIUM(GLsizei width,
                                     GLsizei height,
                                     GLenum internalformat,
                                     GLenum usage) override;
  virtual void DestroyImageCHROMIUM(GLuint image_id) override;
  virtual void* MapImageCHROMIUM(GLuint image_id) override;
  virtual void GetImageParameterivCHROMIUM(GLuint image_id,
                                           GLenum pname,
                                           GLint* params) override;
  virtual void UnmapImageCHROMIUM(GLuint image_id) override;
  virtual GLuint CreateGpuMemoryBufferImageCHROMIUM(GLsizei width,
                                                    GLsizei height,
                                                    GLenum internalformat,
                                                    GLenum usage) override;
  virtual void BindTexImage2DCHROMIUM(GLenum target, GLint image_id) override;
  virtual void ReleaseTexImage2DCHROMIUM(GLenum target,
                                         GLint image_id) override;
  virtual void FramebufferRenderbuffer(GLenum target,
                                       GLenum attachment,
                                       GLenum renderbuffertarget,
                                       GLuint renderbuffer) override;
  virtual void FramebufferTexture2D(GLenum target,
                                    GLenum attachment,
                                    GLenum textarget,
                                    GLuint texture,
                                    GLint level) override;
  virtual void RenderbufferStorage(GLenum target,
                                   GLenum internalformat,
                                   GLsizei width,
                                   GLsizei height) override;

  virtual void* MapBufferCHROMIUM(GLuint target, GLenum access) override;
  virtual GLboolean UnmapBufferCHROMIUM(GLuint target) override;
  virtual void BufferData(GLenum target,
                          GLsizeiptr size,
                          const void* data,
                          GLenum usage) override;

  virtual void WaitSyncPointCHROMIUM(GLuint sync_point) override;
  virtual GLuint InsertSyncPointCHROMIUM() override;

  virtual void BeginQueryEXT(GLenum target, GLuint id) override;
  virtual void EndQueryEXT(GLenum target) override;
  virtual void GetQueryObjectuivEXT(GLuint id,
                                    GLenum pname,
                                    GLuint* params) override;

  virtual void DiscardFramebufferEXT(GLenum target,
                                     GLsizei count,
                                     const GLenum* attachments) override;
  virtual void GenMailboxCHROMIUM(GLbyte* mailbox) override;
  virtual void ProduceTextureCHROMIUM(GLenum target,
                                      const GLbyte* mailbox) override;
  virtual void ProduceTextureDirectCHROMIUM(GLuint texture,
                                            GLenum target,
                                            const GLbyte* mailbox) override;
  virtual void ConsumeTextureCHROMIUM(GLenum target,
                                      const GLbyte* mailbox) override;
  virtual GLuint CreateAndConsumeTextureCHROMIUM(
      GLenum target,
      const GLbyte* mailbox) override;

  virtual void ResizeCHROMIUM(GLuint width,
                              GLuint height,
                              float device_scale) override;
  virtual void LoseContextCHROMIUM(GLenum current, GLenum other) override;

 private:
  TestWebGraphicsContext3D* test_context_;
};

}  // namespace cc

#endif  // CC_TEST_TEST_GLES2_INTERFACE_H_
