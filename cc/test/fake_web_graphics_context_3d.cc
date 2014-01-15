// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_web_graphics_context_3d.h"

#include "base/logging.h"
#include "third_party/khronos/GLES2/gl2.h"

namespace cc {

FakeWebGraphicsContext3D::FakeWebGraphicsContext3D() {}

FakeWebGraphicsContext3D::~FakeWebGraphicsContext3D() {}

bool FakeWebGraphicsContext3D::makeContextCurrent() {
  return true;
}

bool FakeWebGraphicsContext3D::isGLES2Compliant() {
  return false;
}

GLuint FakeWebGraphicsContext3D::getPlatformTextureId() {
  return 0;
}

bool FakeWebGraphicsContext3D::isContextLost() {
  return false;
}

void* FakeWebGraphicsContext3D::mapBufferSubDataCHROMIUM(
    GLenum target,
    GLintptr offset,
    GLsizeiptr size,
    GLenum access) {
  return 0;
}

void* FakeWebGraphicsContext3D::mapTexSubImage2DCHROMIUM(
    GLenum target,
    GLint level,
    GLint xoffset,
    GLint yoffset,
    GLsizei width,
    GLsizei height,
    GLenum format,
    GLenum type,
    GLenum access) {
  return 0;
}

GLenum FakeWebGraphicsContext3D::checkFramebufferStatus(
    GLenum target) {
  return GL_FRAMEBUFFER_COMPLETE;
}

GLint FakeWebGraphicsContext3D::getAttribLocation(
    GLuint program,
    const GLchar* name) {
  return 0;
}

GLenum FakeWebGraphicsContext3D::getError() {
  return GL_NO_ERROR;
}

void FakeWebGraphicsContext3D::getIntegerv(
    GLenum pname,
    GLint* value) {
  if (pname == GL_MAX_TEXTURE_SIZE)
    *value = 1024;
  else if (pname == GL_ACTIVE_TEXTURE)
    *value = GL_TEXTURE0;
}

void FakeWebGraphicsContext3D::getProgramiv(
    GLuint program,
    GLenum pname,
    GLint* value) {
  if (pname == GL_LINK_STATUS)
    *value = 1;
}

void FakeWebGraphicsContext3D::getShaderiv(
    GLuint shader,
    GLenum pname,
    GLint* value) {
  if (pname == GL_COMPILE_STATUS)
    *value = 1;
}

void FakeWebGraphicsContext3D::getShaderPrecisionFormat(
    GLenum shadertype,
    GLenum precisiontype,
    GLint* range,
    GLint* precision) {
  // Return the minimum precision requirements of the GLES specificatin.
  switch (precisiontype) {
    case GL_LOW_INT:
      range[0] = 8;
      range[1] = 8;
      *precision = 0;
      break;
    case GL_MEDIUM_INT:
      range[0] = 10;
      range[1] = 10;
      *precision = 0;
      break;
    case GL_HIGH_INT:
      range[0] = 16;
      range[1] = 16;
      *precision = 0;
      break;
    case GL_LOW_FLOAT:
      range[0] = 8;
      range[1] = 8;
      *precision = 8;
      break;
    case GL_MEDIUM_FLOAT:
      range[0] = 14;
      range[1] = 14;
      *precision = 10;
      break;
    case GL_HIGH_FLOAT:
      range[0] = 62;
      range[1] = 62;
      *precision = 16;
      break;
    default:
      NOTREACHED();
      break;
  }
}

GLint FakeWebGraphicsContext3D::getUniformLocation(
    GLuint program,
    const GLchar* name) {
  return 0;
}

GLsizeiptr FakeWebGraphicsContext3D::getVertexAttribOffset(
    GLuint index,
    GLenum pname) {
  return 0;
}

GLboolean FakeWebGraphicsContext3D::isBuffer(
    GLuint buffer) {
  return false;
}

GLboolean FakeWebGraphicsContext3D::isEnabled(
    GLenum cap) {
  return false;
}

GLboolean FakeWebGraphicsContext3D::isFramebuffer(
    GLuint framebuffer) {
  return false;
}

GLboolean FakeWebGraphicsContext3D::isProgram(
    GLuint program) {
  return false;
}

GLboolean FakeWebGraphicsContext3D::isRenderbuffer(
    GLuint renderbuffer) {
  return false;
}

GLboolean FakeWebGraphicsContext3D::isShader(
    GLuint shader) {
  return false;
}

GLboolean FakeWebGraphicsContext3D::isTexture(
    GLuint texture) {
  return false;
}

void FakeWebGraphicsContext3D::genBuffers(GLsizei count, GLuint* ids) {
  for (int i = 0; i < count; ++i)
    ids[i] = 1;
}

void FakeWebGraphicsContext3D::genFramebuffers(
    GLsizei count, GLuint* ids) {
  for (int i = 0; i < count; ++i)
    ids[i] = 1;
}

void FakeWebGraphicsContext3D::genRenderbuffers(
    GLsizei count, GLuint* ids) {
  for (int i = 0; i < count; ++i)
    ids[i] = 1;
}

void FakeWebGraphicsContext3D::genTextures(GLsizei count, GLuint* ids) {
  for (int i = 0; i < count; ++i)
    ids[i] = 1;
}

void FakeWebGraphicsContext3D::deleteBuffers(GLsizei count, GLuint* ids) {
}

void FakeWebGraphicsContext3D::deleteFramebuffers(
    GLsizei count, GLuint* ids) {
}

void FakeWebGraphicsContext3D::deleteRenderbuffers(
    GLsizei count, GLuint* ids) {
}

void FakeWebGraphicsContext3D::deleteTextures(GLsizei count, GLuint* ids) {
}

GLuint FakeWebGraphicsContext3D::createBuffer() {
  return 1;
}

GLuint FakeWebGraphicsContext3D::createFramebuffer() {
  return 1;
}

GLuint FakeWebGraphicsContext3D::createRenderbuffer() {
  return 1;
}

GLuint FakeWebGraphicsContext3D::createTexture() {
  return 1;
}

void FakeWebGraphicsContext3D::deleteBuffer(GLuint id) {
}

void FakeWebGraphicsContext3D::deleteFramebuffer(GLuint id) {
}

void FakeWebGraphicsContext3D::deleteRenderbuffer(GLuint id) {
}

void FakeWebGraphicsContext3D::deleteTexture(GLuint texture_id) {
}

GLuint FakeWebGraphicsContext3D::createProgram() {
  return 1;
}

GLuint FakeWebGraphicsContext3D::createShader(GLenum) {
  return 1;
}

void FakeWebGraphicsContext3D::deleteProgram(GLuint id) {
}

void FakeWebGraphicsContext3D::deleteShader(GLuint id) {
}

void FakeWebGraphicsContext3D::attachShader(GLuint program, GLuint shader) {
}

void FakeWebGraphicsContext3D::useProgram(GLuint program) {
}

void FakeWebGraphicsContext3D::bindBuffer(GLenum target, GLuint buffer) {
}

void FakeWebGraphicsContext3D::bindFramebuffer(
    GLenum target, GLuint framebuffer) {
}

void FakeWebGraphicsContext3D::bindRenderbuffer(
      GLenum target, GLuint renderbuffer) {
}

void FakeWebGraphicsContext3D::bindTexture(
    GLenum target, GLuint texture_id) {
}

GLuint FakeWebGraphicsContext3D::createQueryEXT() {
  return 1;
}

GLboolean FakeWebGraphicsContext3D::isQueryEXT(GLuint query) {
  return true;
}

void FakeWebGraphicsContext3D::endQueryEXT(GLenum target) {
}

void FakeWebGraphicsContext3D::getQueryObjectuivEXT(
    GLuint query,
    GLenum pname,
    GLuint* params) {
}

void FakeWebGraphicsContext3D::loseContextCHROMIUM(GLenum current,
                                                   GLenum other) {
}

GLuint FakeWebGraphicsContext3D::createImageCHROMIUM(
     GLsizei width, GLsizei height,
     GLenum internalformat) {
  return 0;
}

void* FakeWebGraphicsContext3D::mapImageCHROMIUM(GLuint image_id,
                                                 GLenum access) {
  return 0;
}

}  // namespace cc
