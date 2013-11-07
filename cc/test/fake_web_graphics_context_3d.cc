// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_web_graphics_context_3d.h"

#include "base/logging.h"
#include "third_party/khronos/GLES2/gl2.h"

using blink::WGC3Dboolean;
using blink::WGC3Denum;
using blink::WGC3Dsizei;
using blink::WebGLId;
using blink::WebGraphicsContext3D;

namespace cc {

FakeWebGraphicsContext3D::FakeWebGraphicsContext3D()
    : blink::WebGraphicsContext3D() {
}

FakeWebGraphicsContext3D::~FakeWebGraphicsContext3D() {
}

bool FakeWebGraphicsContext3D::makeContextCurrent() {
  return true;
}

bool FakeWebGraphicsContext3D::isGLES2Compliant() {
  return false;
}

WebGLId FakeWebGraphicsContext3D::getPlatformTextureId() {
  return 0;
}

bool FakeWebGraphicsContext3D::isContextLost() {
  return false;
}

void* FakeWebGraphicsContext3D::mapBufferSubDataCHROMIUM(
    WGC3Denum target,
    blink::WGC3Dintptr offset,
    blink::WGC3Dsizeiptr size,
    WGC3Denum access) {
  return 0;
}

void* FakeWebGraphicsContext3D::mapTexSubImage2DCHROMIUM(
    WGC3Denum target,
    blink::WGC3Dint level,
    blink::WGC3Dint xoffset,
    blink::WGC3Dint yoffset,
    blink::WGC3Dsizei width,
    blink::WGC3Dsizei height,
    WGC3Denum format,
    WGC3Denum type,
    WGC3Denum access) {
  return 0;
}

blink::WebString FakeWebGraphicsContext3D::getRequestableExtensionsCHROMIUM() {
  return blink::WebString();
}

WGC3Denum FakeWebGraphicsContext3D::checkFramebufferStatus(
    WGC3Denum target) {
  return GL_FRAMEBUFFER_COMPLETE;
}

bool FakeWebGraphicsContext3D::getActiveAttrib(
    WebGLId program,
    blink::WGC3Duint index,
    ActiveInfo&) {
  return false;
}

bool FakeWebGraphicsContext3D::getActiveUniform(
    WebGLId program,
    blink::WGC3Duint index,
    ActiveInfo&) {
  return false;
}

blink::WGC3Dint FakeWebGraphicsContext3D::getAttribLocation(
    WebGLId program,
    const blink::WGC3Dchar* name) {
  return 0;
}

WebGraphicsContext3D::Attributes
    FakeWebGraphicsContext3D::getContextAttributes() {
  return WebGraphicsContext3D::Attributes();
}

WGC3Denum FakeWebGraphicsContext3D::getError() {
  return 0;
}

void FakeWebGraphicsContext3D::getIntegerv(
    WGC3Denum pname,
    blink::WGC3Dint* value) {
  if (pname == GL_MAX_TEXTURE_SIZE)
    *value = 1024;
  else if (pname == GL_ACTIVE_TEXTURE)
    *value = GL_TEXTURE0;
}

void FakeWebGraphicsContext3D::getProgramiv(
    WebGLId program,
    WGC3Denum pname,
    blink::WGC3Dint* value) {
  if (pname == GL_LINK_STATUS)
    *value = 1;
}

blink::WebString FakeWebGraphicsContext3D::getProgramInfoLog(
    WebGLId program) {
  return blink::WebString();
}

void FakeWebGraphicsContext3D::getShaderiv(
    WebGLId shader,
    WGC3Denum pname,
    blink::WGC3Dint* value) {
  if (pname == GL_COMPILE_STATUS)
    *value = 1;
}

blink::WebString FakeWebGraphicsContext3D::getShaderInfoLog(
    WebGLId shader) {
  return blink::WebString();
}

void FakeWebGraphicsContext3D::getShaderPrecisionFormat(
    blink::WGC3Denum shadertype,
    blink::WGC3Denum precisiontype,
    blink::WGC3Dint* range,
    blink::WGC3Dint* precision) {
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

blink::WebString FakeWebGraphicsContext3D::getShaderSource(
    WebGLId shader) {
  return blink::WebString();
}

blink::WebString FakeWebGraphicsContext3D::getString(WGC3Denum name) {
  return blink::WebString();
}

blink::WGC3Dint FakeWebGraphicsContext3D::getUniformLocation(
    WebGLId program,
    const blink::WGC3Dchar* name) {
  return 0;
}

blink::WGC3Dsizeiptr FakeWebGraphicsContext3D::getVertexAttribOffset(
    blink::WGC3Duint index,
    WGC3Denum pname) {
  return 0;
}

WGC3Dboolean FakeWebGraphicsContext3D::isBuffer(
    WebGLId buffer) {
  return false;
}

WGC3Dboolean FakeWebGraphicsContext3D::isEnabled(
    WGC3Denum cap) {
  return false;
}

WGC3Dboolean FakeWebGraphicsContext3D::isFramebuffer(
    WebGLId framebuffer) {
  return false;
}

WGC3Dboolean FakeWebGraphicsContext3D::isProgram(
    WebGLId program) {
  return false;
}

WGC3Dboolean FakeWebGraphicsContext3D::isRenderbuffer(
    WebGLId renderbuffer) {
  return false;
}

WGC3Dboolean FakeWebGraphicsContext3D::isShader(
    WebGLId shader) {
  return false;
}

WGC3Dboolean FakeWebGraphicsContext3D::isTexture(
    WebGLId texture) {
  return false;
}

void FakeWebGraphicsContext3D::genBuffers(WGC3Dsizei count, WebGLId* ids) {
  for (int i = 0; i < count; ++i)
    ids[i] = 1;
}

void FakeWebGraphicsContext3D::genFramebuffers(
    WGC3Dsizei count, WebGLId* ids) {
  for (int i = 0; i < count; ++i)
    ids[i] = 1;
}

void FakeWebGraphicsContext3D::genRenderbuffers(
    WGC3Dsizei count, WebGLId* ids) {
  for (int i = 0; i < count; ++i)
    ids[i] = 1;
}

void FakeWebGraphicsContext3D::genTextures(WGC3Dsizei count, WebGLId* ids) {
  for (int i = 0; i < count; ++i)
    ids[i] = 1;
}

void FakeWebGraphicsContext3D::deleteBuffers(WGC3Dsizei count, WebGLId* ids) {
}

void FakeWebGraphicsContext3D::deleteFramebuffers(
    WGC3Dsizei count, WebGLId* ids) {
}

void FakeWebGraphicsContext3D::deleteRenderbuffers(
    WGC3Dsizei count, WebGLId* ids) {
}

void FakeWebGraphicsContext3D::deleteTextures(WGC3Dsizei count, WebGLId* ids) {
}

WebGLId FakeWebGraphicsContext3D::createBuffer() {
  return 1;
}

WebGLId FakeWebGraphicsContext3D::createFramebuffer() {
  return 1;
}

WebGLId FakeWebGraphicsContext3D::createRenderbuffer() {
  return 1;
}

WebGLId FakeWebGraphicsContext3D::createTexture() {
  return 1;
}

void FakeWebGraphicsContext3D::deleteBuffer(blink::WebGLId id) {
}

void FakeWebGraphicsContext3D::deleteFramebuffer(blink::WebGLId id) {
}

void FakeWebGraphicsContext3D::deleteRenderbuffer(blink::WebGLId id) {
}

void FakeWebGraphicsContext3D::deleteTexture(WebGLId texture_id) {
}

WebGLId FakeWebGraphicsContext3D::createProgram() {
  return 1;
}

WebGLId FakeWebGraphicsContext3D::createShader(WGC3Denum) {
  return 1;
}

void FakeWebGraphicsContext3D::deleteProgram(blink::WebGLId id) {
}

void FakeWebGraphicsContext3D::deleteShader(blink::WebGLId id) {
}

void FakeWebGraphicsContext3D::attachShader(WebGLId program, WebGLId shader) {
}

void FakeWebGraphicsContext3D::useProgram(WebGLId program) {
}

void FakeWebGraphicsContext3D::bindBuffer(WGC3Denum target, WebGLId buffer) {
}

void FakeWebGraphicsContext3D::bindFramebuffer(
    WGC3Denum target, WebGLId framebuffer) {
}

void FakeWebGraphicsContext3D::bindRenderbuffer(
      WGC3Denum target, WebGLId renderbuffer) {
}

void FakeWebGraphicsContext3D::bindTexture(
    WGC3Denum target, WebGLId texture_id) {
}

WebGLId FakeWebGraphicsContext3D::createQueryEXT() {
  return 1;
}

WGC3Dboolean FakeWebGraphicsContext3D::isQueryEXT(WebGLId query) {
  return true;
}

void FakeWebGraphicsContext3D::endQueryEXT(blink::WGC3Denum target) {
}

void FakeWebGraphicsContext3D::getQueryObjectuivEXT(
    blink::WebGLId query,
    blink::WGC3Denum pname,
    blink::WGC3Duint* params) {
}

void FakeWebGraphicsContext3D::setContextLostCallback(
    WebGraphicsContextLostCallback* callback) {
}

void FakeWebGraphicsContext3D::loseContextCHROMIUM(WGC3Denum current,
                                                   WGC3Denum other) {
}

blink::WGC3Duint FakeWebGraphicsContext3D::createImageCHROMIUM(
     blink::WGC3Dsizei width, blink::WGC3Dsizei height,
     blink::WGC3Denum internalformat) {
  return 0;
}

void* FakeWebGraphicsContext3D::mapImageCHROMIUM(blink::WGC3Duint image_id,
                                                 blink::WGC3Denum access) {
  return 0;
}

}  // namespace cc
