// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/test_gles2_interface.h"

#include "base/logging.h"
#include "cc/test/test_web_graphics_context_3d.h"

namespace cc {

TestGLES2Interface::TestGLES2Interface(TestWebGraphicsContext3D* test_context)
    : test_context_(test_context) {
  DCHECK(test_context_);
}

TestGLES2Interface::~TestGLES2Interface() {}

void TestGLES2Interface::GenTextures(GLsizei n, GLuint* textures) {
  for (GLsizei i = 0; i < n; ++i) {
    textures[i] = test_context_->createTexture();
  }
}

void TestGLES2Interface::GenBuffers(GLsizei n, GLuint* buffers) {
  for (GLsizei i = 0; i < n; ++i) {
    buffers[i] = test_context_->createBuffer();
  }
}

void TestGLES2Interface::GenFramebuffers(GLsizei n, GLuint* framebuffers) {
  for (GLsizei i = 0; i < n; ++i) {
    framebuffers[i] = test_context_->createFramebuffer();
  }
}

void TestGLES2Interface::GenQueriesEXT(GLsizei n, GLuint* queries) {
  for (GLsizei i = 0; i < n; ++i) {
    queries[i] = test_context_->createQueryEXT();
  }
}

void TestGLES2Interface::DeleteTextures(GLsizei n, const GLuint* textures) {
  for (GLsizei i = 0; i < n; ++i) {
    test_context_->deleteTexture(textures[i]);
  }
}

void TestGLES2Interface::DeleteBuffers(GLsizei n, const GLuint* buffers) {
  for (GLsizei i = 0; i < n; ++i) {
    test_context_->deleteBuffer(buffers[i]);
  }
}

void TestGLES2Interface::DeleteFramebuffers(GLsizei n,
                                            const GLuint* framebuffers) {
  for (GLsizei i = 0; i < n; ++i) {
    test_context_->deleteFramebuffer(framebuffers[i]);
  }
}

void TestGLES2Interface::DeleteQueriesEXT(GLsizei n, const GLuint* queries) {
  for (GLsizei i = 0; i < n; ++i) {
    test_context_->deleteQueryEXT(queries[i]);
  }
}

GLuint TestGLES2Interface::CreateShader(GLenum type) {
  return test_context_->createShader(type);
}

GLuint TestGLES2Interface::CreateProgram() {
  return test_context_->createProgram();
}

void TestGLES2Interface::BindTexture(GLenum target, GLuint texture) {
  test_context_->bindTexture(target, texture);
}

void TestGLES2Interface::GetShaderiv(GLuint shader,
                                     GLenum pname,
                                     GLint* params) {
  test_context_->getShaderiv(shader, pname, params);
}

void TestGLES2Interface::GetProgramiv(GLuint program,
                                      GLenum pname,
                                      GLint* params) {
  test_context_->getProgramiv(program, pname, params);
}

void TestGLES2Interface::GetShaderPrecisionFormat(GLenum shadertype,
                                                  GLenum precisiontype,
                                                  GLint* range,
                                                  GLint* precision) {
  test_context_->getShaderPrecisionFormat(
      shadertype, precisiontype, range, precision);
}

void TestGLES2Interface::Viewport(GLint x,
                                  GLint y,
                                  GLsizei width,
                                  GLsizei height) {
  test_context_->viewport(x, y, width, height);
}

void TestGLES2Interface::ActiveTexture(GLenum target) {
  test_context_->activeTexture(target);
}

void TestGLES2Interface::UseProgram(GLuint program) {
  test_context_->useProgram(program);
}

GLenum TestGLES2Interface::CheckFramebufferStatus(GLenum target) {
  return test_context_->checkFramebufferStatus(target);
}

void TestGLES2Interface::Scissor(GLint x,
                                 GLint y,
                                 GLsizei width,
                                 GLsizei height) {
  test_context_->scissor(x, y, width, height);
}

void TestGLES2Interface::DrawElements(GLenum mode,
                                      GLsizei count,
                                      GLenum type,
                                      const void* indices) {
  test_context_->drawElements(
      mode, count, type, reinterpret_cast<intptr_t>(indices));
}

void TestGLES2Interface::ClearColor(GLclampf red,
                                    GLclampf green,
                                    GLclampf blue,
                                    GLclampf alpha) {
  test_context_->clearColor(red, green, blue, alpha);
}

void TestGLES2Interface::ClearStencil(GLint s) {
  test_context_->clearStencil(s);
}

void TestGLES2Interface::Clear(GLbitfield mask) { test_context_->clear(mask); }

void TestGLES2Interface::Flush() { test_context_->flush(); }

void TestGLES2Interface::Finish() { test_context_->finish(); }

void TestGLES2Interface::Enable(GLenum cap) { test_context_->enable(cap); }

void TestGLES2Interface::Disable(GLenum cap) { test_context_->disable(cap); }

void TestGLES2Interface::BindFramebuffer(GLenum target, GLuint buffer) {
  test_context_->bindFramebuffer(target, buffer);
}

void TestGLES2Interface::BindBuffer(GLenum target, GLuint buffer) {
  test_context_->bindBuffer(target, buffer);
}

void* TestGLES2Interface::MapBufferCHROMIUM(GLuint target, GLenum access) {
  return test_context_->mapBufferCHROMIUM(target, access);
}

GLboolean TestGLES2Interface::UnmapBufferCHROMIUM(GLuint target) {
  return test_context_->unmapBufferCHROMIUM(target);
}

void TestGLES2Interface::BufferData(GLenum target,
                                    GLsizeiptr size,
                                    const void* data,
                                    GLenum usage) {
  test_context_->bufferData(target, size, data, usage);
}

void TestGLES2Interface::WaitSyncPointCHROMIUM(GLuint sync_point) {
  test_context_->waitSyncPoint(sync_point);
}

GLuint TestGLES2Interface::InsertSyncPointCHROMIUM() {
  return test_context_->insertSyncPoint();
}

void TestGLES2Interface::BeginQueryEXT(GLenum target, GLuint id) {
  test_context_->beginQueryEXT(target, id);
}

void TestGLES2Interface::EndQueryEXT(GLenum target) {
  test_context_->endQueryEXT(target);
}

void TestGLES2Interface::DiscardFramebufferEXT(GLenum target,
                                               GLsizei count,
                                               const GLenum* attachments) {
  test_context_->discardFramebufferEXT(target, count, attachments);
}

void TestGLES2Interface::GenMailboxCHROMIUM(GLbyte* mailbox) {
  test_context_->genMailboxCHROMIUM(mailbox);
}

}  // namespace cc
