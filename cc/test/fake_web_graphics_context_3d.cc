// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_web_graphics_context_3d.h"

namespace cc {

bool FakeWebGraphicsContext3D::makeContextCurrent() {
  return true;
}

int FakeWebGraphicsContext3D::width() {
  return 0;
}

int FakeWebGraphicsContext3D::height() {
  return 0;
}

bool FakeWebGraphicsContext3D::isGLES2Compliant() {
  return false;
}

bool FakeWebGraphicsContext3D::readBackFramebuffer(
    unsigned char* pixels,
    size_t bufferSize,
    WebKit::WebGLId framebuffer,
    int width,
    int height) {
  return false;
}

WebKit::WebGLId FakeWebGraphicsContext3D::getPlatformTextureId() {
  return 0;
}

bool FakeWebGraphicsContext3D::isContextLost() {
  return false;
}

void* FakeWebGraphicsContext3D::mapBufferSubDataCHROMIUM(
    WebKit::WGC3Denum target,
    WebKit::WGC3Dintptr offset,
    WebKit::WGC3Dsizeiptr size,
    WebKit::WGC3Denum access) {
  return 0;
}

void* FakeWebGraphicsContext3D::mapTexSubImage2DCHROMIUM(
    WebKit::WGC3Denum target,
    WebKit::WGC3Dint level,
    WebKit::WGC3Dint xoffset,
    WebKit::WGC3Dint yoffset,
    WebKit::WGC3Dsizei width,
    WebKit::WGC3Dsizei height,
    WebKit::WGC3Denum format,
    WebKit::WGC3Denum type,
    WebKit::WGC3Denum access) {
  return 0;
}

WebKit::WebString FakeWebGraphicsContext3D::getRequestableExtensionsCHROMIUM() {
  return WebKit::WebString();
}

WebKit::WGC3Denum FakeWebGraphicsContext3D::checkFramebufferStatus(
    WebKit::WGC3Denum target) {
  return GL_FRAMEBUFFER_COMPLETE;
}

bool FakeWebGraphicsContext3D::getActiveAttrib(
    WebKit::WebGLId program,
    WebKit::WGC3Duint index,
    ActiveInfo&) {
  return false;
}

bool FakeWebGraphicsContext3D::getActiveUniform(
    WebKit::WebGLId program,
    WebKit::WGC3Duint index,
    ActiveInfo&) {
  return false;
}

WebKit::WGC3Dint FakeWebGraphicsContext3D::getAttribLocation(
    WebKit::WebGLId program,
    const WebKit::WGC3Dchar* name) {
  return 0;
}

WebKit::WebGraphicsContext3D::Attributes
    FakeWebGraphicsContext3D::getContextAttributes() {
  return m_attrs;
}

WebKit::WGC3Denum FakeWebGraphicsContext3D::getError() {
  return 0;
}

void FakeWebGraphicsContext3D::getIntegerv(
    WebKit::WGC3Denum pname,
    WebKit::WGC3Dint* value) {
  if (pname == GL_MAX_TEXTURE_SIZE)
    *value = 1024;
}

void FakeWebGraphicsContext3D::getProgramiv(
    WebKit::WebGLId program,
    WebKit::WGC3Denum pname,
    WebKit::WGC3Dint* value) {
  if (pname == GL_LINK_STATUS)
    *value = 1;
}

WebKit::WebString FakeWebGraphicsContext3D::getProgramInfoLog(
    WebKit::WebGLId program) {
  return WebKit::WebString();
}

void FakeWebGraphicsContext3D::getShaderiv(
    WebKit::WebGLId shader,
    WebKit::WGC3Denum pname,
    WebKit::WGC3Dint* value) {
  if (pname == GL_COMPILE_STATUS)
    *value = 1;
}

WebKit::WebString FakeWebGraphicsContext3D::getShaderInfoLog(
    WebKit::WebGLId shader) {
  return WebKit::WebString();
}

WebKit::WebString FakeWebGraphicsContext3D::getShaderSource(
    WebKit::WebGLId shader) {
  return WebKit::WebString();
}

WebKit::WebString FakeWebGraphicsContext3D::getString(WebKit::WGC3Denum name) {
  return WebKit::WebString();
}

WebKit::WGC3Dint FakeWebGraphicsContext3D::getUniformLocation(
    WebKit::WebGLId program,
    const WebKit::WGC3Dchar* name) {
  return 0;
}

WebKit::WGC3Dsizeiptr FakeWebGraphicsContext3D::getVertexAttribOffset(
    WebKit::WGC3Duint index,
    WebKit::WGC3Denum pname) {
  return 0;
}

WebKit::WGC3Dboolean FakeWebGraphicsContext3D::isBuffer(
    WebKit::WebGLId buffer) {
  return false;
}

WebKit::WGC3Dboolean FakeWebGraphicsContext3D::isEnabled(
    WebKit::WGC3Denum cap) {
  return false;
}

WebKit::WGC3Dboolean FakeWebGraphicsContext3D::isFramebuffer(
    WebKit::WebGLId framebuffer) {
  return false;
}

WebKit::WGC3Dboolean FakeWebGraphicsContext3D::isProgram(
    WebKit::WebGLId program) {
  return false;
}

WebKit::WGC3Dboolean FakeWebGraphicsContext3D::isRenderbuffer(
    WebKit::WebGLId renderbuffer) {
  return false;
}

WebKit::WGC3Dboolean FakeWebGraphicsContext3D::isShader(
    WebKit::WebGLId shader) {
  return false;
}

WebKit::WGC3Dboolean FakeWebGraphicsContext3D::isTexture(
    WebKit::WebGLId texture) {
  return false;
}

WebKit::WebGLId FakeWebGraphicsContext3D::createBuffer() {
  return 1;
}

WebKit::WebGLId FakeWebGraphicsContext3D::createFramebuffer() {
  return 1;
}

WebKit::WebGLId FakeWebGraphicsContext3D::createProgram() {
  return 1;
}

WebKit::WebGLId FakeWebGraphicsContext3D::createRenderbuffer() {
  return 1;
}

WebKit::WebGLId FakeWebGraphicsContext3D::createShader(WebKit::WGC3Denum) {
  return 1;
}

WebKit::WebGLId FakeWebGraphicsContext3D::createTexture() {
  return m_nextTextureId++;
}

WebKit::WebGLId FakeWebGraphicsContext3D::createQueryEXT() {
  return 1;
}

WebKit::WGC3Dboolean FakeWebGraphicsContext3D::isQueryEXT(WebKit::WebGLId) {
  return true;
}

}  // namespace cc
