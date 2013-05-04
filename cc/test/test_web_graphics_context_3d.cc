// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/test_web_graphics_context_3d.h"

#include <algorithm>
#include <string>

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/khronos/GLES2/gl2ext.h"

using WebKit::WGC3Dboolean;
using WebKit::WGC3Dchar;
using WebKit::WGC3Denum;
using WebKit::WGC3Dint;
using WebKit::WGC3Dsizei;
using WebKit::WGC3Dsizeiptr;
using WebKit::WGC3Duint;
using WebKit::WebGLId;
using WebKit::WebGraphicsContext3D;

namespace cc {

static const WebGLId kFramebufferId = 1;
static const WebGLId kProgramId = 2;
static const WebGLId kRenderbufferId = 3;
static const WebGLId kShaderId = 4;

static unsigned s_context_id = 1;

const WebGLId TestWebGraphicsContext3D::kExternalTextureId = 1337;

TestWebGraphicsContext3D::TestWebGraphicsContext3D()
    : FakeWebGraphicsContext3D(),
      context_id_(s_context_id++),
      next_buffer_id_(1),
      next_texture_id_(1),
      have_extension_io_surface_(false),
      have_extension_egl_image_(false),
      times_make_current_succeeds_(-1),
      times_bind_texture_succeeds_(-1),
      times_end_query_succeeds_(-1),
      context_lost_(false),
      context_lost_callback_(NULL),
      max_texture_size_(1024),
      width_(0),
      height_(0),
      bound_buffer_(0) {
}

TestWebGraphicsContext3D::TestWebGraphicsContext3D(
    const WebGraphicsContext3D::Attributes& attributes)
    : FakeWebGraphicsContext3D(),
      context_id_(s_context_id++),
      next_buffer_id_(1),
      next_texture_id_(1),
      attributes_(attributes),
      have_extension_io_surface_(false),
      have_extension_egl_image_(false),
      times_make_current_succeeds_(-1),
      times_bind_texture_succeeds_(-1),
      times_end_query_succeeds_(-1),
      context_lost_(false),
      context_lost_callback_(NULL),
      max_texture_size_(1024),
      width_(0),
      height_(0),
      bound_buffer_(0) {
}

TestWebGraphicsContext3D::~TestWebGraphicsContext3D() {
  for (size_t i = 0; i < sync_point_callbacks_.size(); ++i) {
    if (sync_point_callbacks_[i] != NULL)
      delete sync_point_callbacks_[i];
  }
}

bool TestWebGraphicsContext3D::makeContextCurrent() {
  if (times_make_current_succeeds_ >= 0) {
    if (!times_make_current_succeeds_) {
      loseContextCHROMIUM(GL_GUILTY_CONTEXT_RESET_ARB,
                          GL_INNOCENT_CONTEXT_RESET_ARB);
    }
    --times_make_current_succeeds_;
  }
  return !context_lost_;
}

int TestWebGraphicsContext3D::width() {
  return width_;
}

int TestWebGraphicsContext3D::height() {
  return height_;
}

void TestWebGraphicsContext3D::reshape(int width, int height) {
  width_ = width;
  height_ = height;
}

bool TestWebGraphicsContext3D::isContextLost() {
  return context_lost_;
}

WGC3Denum TestWebGraphicsContext3D::getGraphicsResetStatusARB() {
  return context_lost_ ? GL_UNKNOWN_CONTEXT_RESET_ARB : GL_NO_ERROR;
}

WGC3Denum TestWebGraphicsContext3D::checkFramebufferStatus(
    WGC3Denum target) {
  if (context_lost_)
    return GL_FRAMEBUFFER_UNDEFINED_OES;
  return GL_FRAMEBUFFER_COMPLETE;
}

WebGraphicsContext3D::Attributes
    TestWebGraphicsContext3D::getContextAttributes() {
  return attributes_;
}

WebKit::WebString TestWebGraphicsContext3D::getString(WGC3Denum name) {
  std::string string;

  if (name == GL_EXTENSIONS) {
    if (have_extension_io_surface_)
      string += "GL_CHROMIUM_iosurface GL_ARB_texture_rectangle ";
    if (have_extension_egl_image_)
      string += "GL_OES_EGL_image_external";
  }

  return WebKit::WebString::fromUTF8(string.c_str());
}

WGC3Dint TestWebGraphicsContext3D::getUniformLocation(
    WebGLId program,
    const WGC3Dchar* name) {
  return 0;
}

WGC3Dsizeiptr TestWebGraphicsContext3D::getVertexAttribOffset(
    WGC3Duint index,
    WGC3Denum pname) {
  return 0;
}

WGC3Dboolean TestWebGraphicsContext3D::isBuffer(
    WebGLId buffer) {
  return false;
}

WGC3Dboolean TestWebGraphicsContext3D::isEnabled(
    WGC3Denum cap) {
  return false;
}

WGC3Dboolean TestWebGraphicsContext3D::isFramebuffer(
    WebGLId framebuffer) {
  return false;
}

WGC3Dboolean TestWebGraphicsContext3D::isProgram(
    WebGLId program) {
  return false;
}

WGC3Dboolean TestWebGraphicsContext3D::isRenderbuffer(
    WebGLId renderbuffer) {
  return false;
}

WGC3Dboolean TestWebGraphicsContext3D::isShader(
    WebGLId shader) {
  return false;
}

WGC3Dboolean TestWebGraphicsContext3D::isTexture(
    WebGLId texture) {
  return false;
}

WebGLId TestWebGraphicsContext3D::createBuffer() {
  return NextBufferId();
}

void TestWebGraphicsContext3D::deleteBuffer(WebGLId id) {
  unsigned context_id = id >> 17;
  unsigned buffer_id = id & 0x1ffff;
  DCHECK(buffer_id && buffer_id < next_buffer_id_);
  DCHECK_EQ(context_id, context_id_);
}

WebGLId TestWebGraphicsContext3D::createFramebuffer() {
  return kFramebufferId | context_id_ << 16;
}

void TestWebGraphicsContext3D::deleteFramebuffer(WebGLId id) {
  EXPECT_EQ(kFramebufferId | context_id_ << 16, id);
}

WebGLId TestWebGraphicsContext3D::createProgram() {
  return kProgramId | context_id_ << 16;
}

void TestWebGraphicsContext3D::deleteProgram(WebGLId id) {
  EXPECT_EQ(kProgramId | context_id_ << 16, id);
}

WebGLId TestWebGraphicsContext3D::createRenderbuffer() {
  return kRenderbufferId | context_id_ << 16;
}

void TestWebGraphicsContext3D::deleteRenderbuffer(WebGLId id) {
  EXPECT_EQ(kRenderbufferId | context_id_ << 16, id);
}

WebGLId TestWebGraphicsContext3D::createShader(WGC3Denum) {
  return kShaderId | context_id_ << 16;
}

void TestWebGraphicsContext3D::deleteShader(WebGLId id) {
  EXPECT_EQ(kShaderId | context_id_ << 16, id);
}

WebGLId TestWebGraphicsContext3D::createTexture() {
  WebGLId texture_id = NextTextureId();
  DCHECK_NE(texture_id, kExternalTextureId);
  textures_.push_back(texture_id);
  return texture_id;
}

void TestWebGraphicsContext3D::deleteTexture(WebGLId texture_id) {
  DCHECK(std::find(textures_.begin(), textures_.end(), texture_id) !=
         textures_.end());
  textures_.erase(std::find(textures_.begin(), textures_.end(), texture_id));
}

void TestWebGraphicsContext3D::attachShader(WebGLId program, WebGLId shader) {
  EXPECT_EQ(kProgramId | context_id_ << 16, program);
  EXPECT_EQ(kShaderId | context_id_ << 16, shader);
}

void TestWebGraphicsContext3D::useProgram(WebGLId program) {
  if (!program)
    return;
  EXPECT_EQ(kProgramId | context_id_ << 16, program);
}

void TestWebGraphicsContext3D::bindFramebuffer(
    WGC3Denum target, WebGLId framebuffer) {
  if (!framebuffer)
    return;
  EXPECT_EQ(kFramebufferId | context_id_ << 16, framebuffer);
}

void TestWebGraphicsContext3D::bindRenderbuffer(
      WGC3Denum target, WebGLId renderbuffer) {
  if (!renderbuffer)
    return;
  EXPECT_EQ(kRenderbufferId | context_id_ << 16, renderbuffer);
}

void TestWebGraphicsContext3D::bindTexture(
    WGC3Denum target, WebGLId texture_id) {
  if (times_bind_texture_succeeds_ >= 0) {
    if (!times_bind_texture_succeeds_) {
      loseContextCHROMIUM(GL_GUILTY_CONTEXT_RESET_ARB,
                          GL_INNOCENT_CONTEXT_RESET_ARB);
    }
    --times_bind_texture_succeeds_;
  }

  if (!texture_id)
    return;
  if (texture_id == kExternalTextureId)
    return;
  DCHECK(std::find(textures_.begin(), textures_.end(), texture_id) !=
         textures_.end());
  used_textures_.insert(texture_id);
}

void TestWebGraphicsContext3D::endQueryEXT(WGC3Denum target) {
  if (times_end_query_succeeds_ >= 0) {
    if (!times_end_query_succeeds_) {
      loseContextCHROMIUM(GL_GUILTY_CONTEXT_RESET_ARB,
                          GL_INNOCENT_CONTEXT_RESET_ARB);
    }
    --times_end_query_succeeds_;
  }
}

void TestWebGraphicsContext3D::getQueryObjectuivEXT(
    WebGLId query,
    WGC3Denum pname,
    WGC3Duint* params) {
  // If the context is lost, behave as if result is available.
  if (pname == GL_QUERY_RESULT_AVAILABLE_EXT)
    *params = 1;
}

void TestWebGraphicsContext3D::getIntegerv(
    WGC3Denum pname,
    WebKit::WGC3Dint* value) {
  if (pname == GL_MAX_TEXTURE_SIZE)
    *value = max_texture_size_;
}

void TestWebGraphicsContext3D::genMailboxCHROMIUM(WebKit::WGC3Dbyte* mailbox) {
  static char mailbox_name1 = '1';
  static char mailbox_name2 = '1';
  mailbox[0] = mailbox_name1;
  mailbox[1] = mailbox_name2;
  mailbox[2] = '\0';
  if (++mailbox_name1 == 0) {
    mailbox_name1 = '1';
    ++mailbox_name2;
  }
}

void TestWebGraphicsContext3D::setContextLostCallback(
    WebGraphicsContextLostCallback* callback) {
  context_lost_callback_ = callback;
}

void TestWebGraphicsContext3D::loseContextCHROMIUM(WGC3Denum current,
                                                   WGC3Denum other) {
  if (context_lost_)
    return;
  context_lost_ = true;
  if (context_lost_callback_)
    context_lost_callback_->onContextLost();

  for (size_t i = 0; i < shared_contexts_.size(); ++i)
    shared_contexts_[i]->loseContextCHROMIUM(current, other);
  shared_contexts_.clear();
}

void TestWebGraphicsContext3D::signalSyncPoint(
    unsigned sync_point,
    WebGraphicsSyncPointCallback* callback) {
  sync_point_callbacks_.push_back(callback);
}

void TestWebGraphicsContext3D::prepareTexture() {
  CallAllSyncPointCallbacks();
}

void TestWebGraphicsContext3D::finish() {
  CallAllSyncPointCallbacks();
}

void TestWebGraphicsContext3D::flush() {
  CallAllSyncPointCallbacks();
}

static void CallAndDestroy(
    WebKit::WebGraphicsContext3D::WebGraphicsSyncPointCallback* callback) {
  if (!callback)
    return;
  callback->onSyncPointReached();
  delete callback;
}

void TestWebGraphicsContext3D::CallAllSyncPointCallbacks() {
  for (size_t i = 0; i < sync_point_callbacks_.size(); ++i) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&CallAndDestroy,
                   sync_point_callbacks_[i]));
  }
  sync_point_callbacks_.clear();
}

void TestWebGraphicsContext3D::bindBuffer(WebKit::WGC3Denum target,
                                          WebKit::WebGLId buffer) {
  bound_buffer_ = buffer;
  if (!bound_buffer_)
    return;
  unsigned context_id = buffer >> 17;
  unsigned buffer_id = buffer & 0x1ffff;
  DCHECK(buffer_id && buffer_id < next_buffer_id_);
  DCHECK_EQ(context_id, context_id_);

  if (buffers_.count(bound_buffer_) == 0)
    buffers_.set(bound_buffer_, make_scoped_ptr(new Buffer).Pass());

  buffers_.get(bound_buffer_)->target = target;
}

void TestWebGraphicsContext3D::bufferData(WebKit::WGC3Denum target,
                                          WebKit::WGC3Dsizeiptr size,
                                          const void* data,
                                          WebKit::WGC3Denum usage) {
  DCHECK_GT(buffers_.count(bound_buffer_), 0u);
  DCHECK_EQ(target, buffers_.get(bound_buffer_)->target);
  if (context_lost_) {
    buffers_.get(bound_buffer_)->pixels.reset();
    return;
  }
  buffers_.get(bound_buffer_)->pixels.reset(new uint8[size]);
  if (data != NULL)
    memcpy(buffers_.get(bound_buffer_)->pixels.get(), data, size);
}

void* TestWebGraphicsContext3D::mapBufferCHROMIUM(WebKit::WGC3Denum target,
                                                  WebKit::WGC3Denum access) {
  DCHECK_GT(buffers_.count(bound_buffer_), 0u);
  DCHECK_EQ(target, buffers_.get(bound_buffer_)->target);
  return buffers_.get(bound_buffer_)->pixels.get();
}

WebKit::WGC3Dboolean TestWebGraphicsContext3D::unmapBufferCHROMIUM(
    WebKit::WGC3Denum target) {
  DCHECK_GT(buffers_.count(bound_buffer_), 0u);
  DCHECK_EQ(target, buffers_.get(bound_buffer_)->target);
  buffers_.get(bound_buffer_)->pixels.reset();
  return true;
}

WebGLId TestWebGraphicsContext3D::NextTextureId() {
  WebGLId texture_id = next_texture_id_++;
  DCHECK(texture_id < (1 << 16));
  texture_id |= context_id_ << 16;
  return texture_id;
}

WebGLId TestWebGraphicsContext3D::NextBufferId() {
  WebGLId buffer_id = next_buffer_id_++;
  DCHECK(buffer_id < (1 << 17));
  buffer_id |= context_id_ << 17;
  return buffer_id;
}

TestWebGraphicsContext3D::Buffer::Buffer() : target(0) {}

TestWebGraphicsContext3D::Buffer::~Buffer() {}

}  // namespace cc
