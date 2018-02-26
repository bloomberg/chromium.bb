// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_TEST_TEST_GLES2_INTERFACE_H_
#define COMPONENTS_VIZ_TEST_TEST_GLES2_INTERFACE_H_

#include "gpu/command_buffer/client/gles2_interface_stub.h"

namespace viz {

class TestWebGraphicsContext3D;

class TestGLES2Interface : public gpu::gles2::GLES2InterfaceStub {
 public:
  TestGLES2Interface();
  ~TestGLES2Interface() override;

  void GenTextures(GLsizei n, GLuint* textures) override;
  void GenBuffers(GLsizei n, GLuint* buffers) override;
  void GenFramebuffers(GLsizei n, GLuint* framebuffers) override;
  void GenRenderbuffers(GLsizei n, GLuint* renderbuffers) override;
  void GenQueriesEXT(GLsizei n, GLuint* queries) override;

  void DeleteTextures(GLsizei n, const GLuint* textures) override;
  void DeleteBuffers(GLsizei n, const GLuint* buffers) override;
  void DeleteFramebuffers(GLsizei n, const GLuint* framebuffers) override;
  void DeleteQueriesEXT(GLsizei n, const GLuint* queries) override;

  GLuint CreateShader(GLenum type) override;
  GLuint CreateProgram() override;

  void BindTexture(GLenum target, GLuint texture) override;

  void GetIntegerv(GLenum pname, GLint* params) override;
  void GetShaderiv(GLuint shader, GLenum pname, GLint* params) override;
  void GetProgramiv(GLuint program, GLenum pname, GLint* params) override;
  void GetShaderPrecisionFormat(GLenum shadertype,
                                GLenum precisiontype,
                                GLint* range,
                                GLint* precision) override;
  GLenum CheckFramebufferStatus(GLenum target) override;

  void ActiveTexture(GLenum target) override;
  void Viewport(GLint x, GLint y, GLsizei width, GLsizei height) override;
  void UseProgram(GLuint program) override;
  void Scissor(GLint x, GLint y, GLsizei width, GLsizei height) override;
  void DrawElements(GLenum mode,
                    GLsizei count,
                    GLenum type,
                    const void* indices) override;
  void ClearColor(GLclampf red,
                  GLclampf green,
                  GLclampf blue,
                  GLclampf alpha) override;
  void ClearStencil(GLint s) override;
  void Clear(GLbitfield mask) override;
  void Flush() override;
  void Finish() override;
  void ShallowFinishCHROMIUM() override;
  void ShallowFlushCHROMIUM() override;
  void Enable(GLenum cap) override;
  void Disable(GLenum cap) override;

  void BindBuffer(GLenum target, GLuint buffer) override;
  void BindRenderbuffer(GLenum target, GLuint buffer) override;
  void BindFramebuffer(GLenum target, GLuint buffer) override;

  void PixelStorei(GLenum pname, GLint param) override;

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
  void TexStorage2DEXT(GLenum target,
                       GLsizei levels,
                       GLenum internalformat,
                       GLsizei width,
                       GLsizei height) override;
  void TexStorage2DImageCHROMIUM(GLenum target,
                                 GLenum internalformat,
                                 GLenum bufferusage,
                                 GLsizei width,
                                 GLsizei height) override;
  void TexParameteri(GLenum target, GLenum pname, GLint param) override;

  void CompressedTexImage2D(GLenum target,
                            GLint level,
                            GLenum internalformat,
                            GLsizei width,
                            GLsizei height,
                            GLint border,
                            GLsizei image_size,
                            const void* data) override;
  GLuint CreateImageCHROMIUM(ClientBuffer buffer,
                             GLsizei width,
                             GLsizei height,
                             GLenum internalformat) override;
  void DestroyImageCHROMIUM(GLuint image_id) override;
  void BindTexImage2DCHROMIUM(GLenum target, GLint image_id) override;
  void ReleaseTexImage2DCHROMIUM(GLenum target, GLint image_id) override;
  void FramebufferRenderbuffer(GLenum target,
                               GLenum attachment,
                               GLenum renderbuffertarget,
                               GLuint renderbuffer) override;
  void FramebufferTexture2D(GLenum target,
                            GLenum attachment,
                            GLenum textarget,
                            GLuint texture,
                            GLint level) override;
  void RenderbufferStorage(GLenum target,
                           GLenum internalformat,
                           GLsizei width,
                           GLsizei height) override;

  void* MapBufferCHROMIUM(GLuint target, GLenum access) override;
  GLboolean UnmapBufferCHROMIUM(GLuint target) override;
  void BufferData(GLenum target,
                  GLsizeiptr size,
                  const void* data,
                  GLenum usage) override;

  void GenSyncTokenCHROMIUM(GLbyte* sync_token) override;
  void GenUnverifiedSyncTokenCHROMIUM(GLbyte* sync_token) override;
  void VerifySyncTokensCHROMIUM(GLbyte** sync_tokens, GLsizei count) override;
  void WaitSyncTokenCHROMIUM(const GLbyte* sync_token) override;

  void BeginQueryEXT(GLenum target, GLuint id) override;
  void EndQueryEXT(GLenum target) override;
  void GetQueryObjectuivEXT(GLuint id, GLenum pname, GLuint* params) override;

  void DiscardFramebufferEXT(GLenum target,
                             GLsizei count,
                             const GLenum* attachments) override;
  void GenMailboxCHROMIUM(GLbyte* mailbox) override;
  void ProduceTextureDirectCHROMIUM(GLuint texture,
                                    const GLbyte* mailbox) override;
  GLuint CreateAndConsumeTextureCHROMIUM(const GLbyte* mailbox) override;

  void ResizeCHROMIUM(GLuint width,
                      GLuint height,
                      float device_scale,
                      GLenum color_space,
                      GLboolean has_alpha) override;
  void LoseContextCHROMIUM(GLenum current, GLenum other) override;
  GLenum GetGraphicsResetStatusKHR() override;

  void set_test_context(TestWebGraphicsContext3D* context);

 protected:
  virtual void InitializeTestContext(TestWebGraphicsContext3D* context) {}

 protected:
  TestWebGraphicsContext3D* test_context_ = nullptr;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_TEST_TEST_GLES2_INTERFACE_H_
