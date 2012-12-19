// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_web_graphics_context_3d.h"

#include <algorithm>

#include "base/logging.h"
#include "third_party/khronos/GLES2/gl2ext.h"

using WebKit::WGC3Dboolean;
using WebKit::WGC3Denum;
using WebKit::WebGLId;
using WebKit::WebGraphicsContext3D;

namespace cc {

FakeWebGraphicsContext3D::FakeWebGraphicsContext3D()
    : next_texture_id_(1)
    , context_lost_(false)
    , context_lost_callback_(NULL)
{
}

FakeWebGraphicsContext3D::FakeWebGraphicsContext3D(
    const WebGraphicsContext3D::Attributes& attributes)
    : next_texture_id_(1)
    , attributes_(attributes)
    , context_lost_(false)
    , context_lost_callback_(NULL)
{
}

FakeWebGraphicsContext3D::~FakeWebGraphicsContext3D()
{
}

bool FakeWebGraphicsContext3D::makeContextCurrent() {
  return !context_lost_;
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
    WebGLId framebuffer,
    int width,
    int height) {
  return false;
}

WebGLId FakeWebGraphicsContext3D::getPlatformTextureId() {
  return 0;
}

bool FakeWebGraphicsContext3D::isContextLost() {
  return context_lost_;
}

WGC3Denum FakeWebGraphicsContext3D::getGraphicsResetStatusARB() {
  return context_lost_ ? GL_UNKNOWN_CONTEXT_RESET_ARB : GL_NO_ERROR;
}

void* FakeWebGraphicsContext3D::mapBufferSubDataCHROMIUM(
    WGC3Denum target,
    WebKit::WGC3Dintptr offset,
    WebKit::WGC3Dsizeiptr size,
    WGC3Denum access) {
  return 0;
}

void* FakeWebGraphicsContext3D::mapTexSubImage2DCHROMIUM(
    WGC3Denum target,
    WebKit::WGC3Dint level,
    WebKit::WGC3Dint xoffset,
    WebKit::WGC3Dint yoffset,
    WebKit::WGC3Dsizei width,
    WebKit::WGC3Dsizei height,
    WGC3Denum format,
    WGC3Denum type,
    WGC3Denum access) {
  return 0;
}

WebKit::WebString FakeWebGraphicsContext3D::getRequestableExtensionsCHROMIUM() {
  return WebKit::WebString();
}

WGC3Denum FakeWebGraphicsContext3D::checkFramebufferStatus(
    WGC3Denum target) {
  if (context_lost_)
    return GL_FRAMEBUFFER_UNDEFINED_OES;
  return GL_FRAMEBUFFER_COMPLETE;
}

bool FakeWebGraphicsContext3D::getActiveAttrib(
    WebGLId program,
    WebKit::WGC3Duint index,
    ActiveInfo&) {
  return false;
}

bool FakeWebGraphicsContext3D::getActiveUniform(
    WebGLId program,
    WebKit::WGC3Duint index,
    ActiveInfo&) {
  return false;
}

WebKit::WGC3Dint FakeWebGraphicsContext3D::getAttribLocation(
    WebGLId program,
    const WebKit::WGC3Dchar* name) {
  return 0;
}

WebGraphicsContext3D::Attributes
    FakeWebGraphicsContext3D::getContextAttributes() {
  return attributes_;
}

WGC3Denum FakeWebGraphicsContext3D::getError() {
  return 0;
}

void FakeWebGraphicsContext3D::getIntegerv(
    WGC3Denum pname,
    WebKit::WGC3Dint* value) {
  if (pname == GL_MAX_TEXTURE_SIZE)
    *value = 1024;
}

void FakeWebGraphicsContext3D::getProgramiv(
    WebGLId program,
    WGC3Denum pname,
    WebKit::WGC3Dint* value) {
  if (pname == GL_LINK_STATUS)
    *value = 1;
}

WebKit::WebString FakeWebGraphicsContext3D::getProgramInfoLog(
    WebGLId program) {
  return WebKit::WebString();
}

void FakeWebGraphicsContext3D::getShaderiv(
    WebGLId shader,
    WGC3Denum pname,
    WebKit::WGC3Dint* value) {
  if (pname == GL_COMPILE_STATUS)
    *value = 1;
}

WebKit::WebString FakeWebGraphicsContext3D::getShaderInfoLog(
    WebGLId shader) {
  return WebKit::WebString();
}

WebKit::WebString FakeWebGraphicsContext3D::getShaderSource(
    WebGLId shader) {
  return WebKit::WebString();
}

WebKit::WebString FakeWebGraphicsContext3D::getString(WGC3Denum name) {
  return WebKit::WebString();
}

WebKit::WGC3Dint FakeWebGraphicsContext3D::getUniformLocation(
    WebGLId program,
    const WebKit::WGC3Dchar* name) {
  return 0;
}

WebKit::WGC3Dsizeiptr FakeWebGraphicsContext3D::getVertexAttribOffset(
    WebKit::WGC3Duint index,
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

WebGLId FakeWebGraphicsContext3D::createBuffer() {
  return 1;
}

WebGLId FakeWebGraphicsContext3D::createFramebuffer() {
  return 1;
}

WebGLId FakeWebGraphicsContext3D::createProgram() {
  return 1;
}

WebGLId FakeWebGraphicsContext3D::createRenderbuffer() {
  return 1;
}

WebGLId FakeWebGraphicsContext3D::createShader(WGC3Denum) {
  return 1;
}

WebGLId FakeWebGraphicsContext3D::createTexture() {
  WebGLId texture_id = next_texture_id_++;
  textures_.push_back(texture_id);
  return texture_id;
}

void FakeWebGraphicsContext3D::deleteTexture(WebGLId texture_id) {
  textures_.erase(std::find(textures_.begin(), textures_.end(), texture_id));
}

void FakeWebGraphicsContext3D::bindTexture(
    WGC3Denum target, WebGLId texture_id) {
  if (!texture_id)
    return;
  DCHECK(std::find(textures_.begin(), textures_.end(), texture_id) !=
         textures_.end());
  used_textures_.insert(texture_id);
}

WebGLId FakeWebGraphicsContext3D::createQueryEXT() {
  return 1;
}

WGC3Dboolean FakeWebGraphicsContext3D::isQueryEXT(WebGLId query) {
  return true;
}

void FakeWebGraphicsContext3D::SetContextLostCallback(
    WebGraphicsContextLostCallback* callback) {
  context_lost_callback_ = callback;
}

void FakeWebGraphicsContext3D::loseContextCHROMIUM() {
  context_lost_ = true;
  if (context_lost_callback_)
    context_lost_callback_->onContextLost();
}

}  // namespace cc
