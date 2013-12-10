// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/test_web_graphics_context_3d.h"

#include <algorithm>
#include <string>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "cc/test/test_context_support.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/khronos/GLES2/gl2ext.h"

using blink::WGC3Dboolean;
using blink::WGC3Dchar;
using blink::WGC3Denum;
using blink::WGC3Dint;
using blink::WGC3Dsizei;
using blink::WGC3Dsizeiptr;
using blink::WGC3Duint;
using blink::WebGLId;
using blink::WebGraphicsContext3D;

namespace cc {

static const WebGLId kFramebufferId = 1;
static const WebGLId kRenderbufferId = 2;

static unsigned s_context_id = 1;

const WebGLId TestWebGraphicsContext3D::kExternalTextureId = 1337;

static base::LazyInstance<base::Lock>::Leaky
    g_shared_namespace_lock = LAZY_INSTANCE_INITIALIZER;

TestWebGraphicsContext3D::Namespace*
    TestWebGraphicsContext3D::shared_namespace_ = NULL;

TestWebGraphicsContext3D::Namespace::Namespace()
    : next_buffer_id(1),
      next_image_id(1),
      next_texture_id(1) {
}

TestWebGraphicsContext3D::Namespace::~Namespace() {
  g_shared_namespace_lock.Get().AssertAcquired();
  if (shared_namespace_ == this)
    shared_namespace_ = NULL;
}

// static
scoped_ptr<TestWebGraphicsContext3D> TestWebGraphicsContext3D::Create() {
  return make_scoped_ptr(new TestWebGraphicsContext3D());
}

TestWebGraphicsContext3D::TestWebGraphicsContext3D()
    : FakeWebGraphicsContext3D(),
      context_id_(s_context_id++),
      times_bind_texture_succeeds_(-1),
      times_end_query_succeeds_(-1),
      times_gen_mailbox_succeeds_(-1),
      context_lost_(false),
      times_map_image_chromium_succeeds_(-1),
      times_map_buffer_chromium_succeeds_(-1),
      context_lost_callback_(NULL),
      next_program_id_(1000),
      next_shader_id_(2000),
      max_texture_size_(2048),
      reshape_called_(false),
      width_(0),
      height_(0),
      scale_factor_(-1.f),
      test_support_(NULL),
      last_update_type_(NoUpdate),
      next_insert_sync_point_(1),
      last_waited_sync_point_(0),
      bound_buffer_(0),
      peak_transfer_buffer_memory_used_bytes_(0),
      weak_ptr_factory_(this) {
  CreateNamespace();
}

TestWebGraphicsContext3D::~TestWebGraphicsContext3D() {
  base::AutoLock lock(g_shared_namespace_lock.Get());
  namespace_ = NULL;
}

void TestWebGraphicsContext3D::CreateNamespace() {
  if (attributes_.shareResources) {
    base::AutoLock lock(g_shared_namespace_lock.Get());
    if (shared_namespace_) {
      namespace_ = shared_namespace_;
    } else {
      namespace_ = new Namespace;
      shared_namespace_ = namespace_.get();
    }
  } else {
    namespace_ = new Namespace;
  }
}

void TestWebGraphicsContext3D::reshapeWithScaleFactor(
    int width, int height, float scale_factor) {
  reshape_called_ = true;
  width_ = width;
  height_ = height;
  scale_factor_ = scale_factor;
}

bool TestWebGraphicsContext3D::isContextLost() {
  return context_lost_;
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

blink::WebString TestWebGraphicsContext3D::getString(WGC3Denum name) {
  return blink::WebString();
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

void TestWebGraphicsContext3D::genBuffers(WGC3Dsizei count, WebGLId* ids) {
  for (int i = 0; i < count; ++i)
    ids[i] = NextBufferId();
}

void TestWebGraphicsContext3D::genFramebuffers(
    WGC3Dsizei count, WebGLId* ids) {
  for (int i = 0; i < count; ++i)
    ids[i] = kFramebufferId | context_id_ << 16;
}

void TestWebGraphicsContext3D::genRenderbuffers(
    WGC3Dsizei count, WebGLId* ids) {
  for (int i = 0; i < count; ++i)
    ids[i] = kRenderbufferId | context_id_ << 16;
}

void TestWebGraphicsContext3D::genTextures(WGC3Dsizei count, WebGLId* ids) {
  for (int i = 0; i < count; ++i) {
    ids[i] = NextTextureId();
    DCHECK_NE(ids[i], kExternalTextureId);
  }
  base::AutoLock lock(namespace_->lock);
  for (int i = 0; i < count; ++i)
    namespace_->textures.Append(ids[i], new TestTexture());
}

void TestWebGraphicsContext3D::deleteBuffers(WGC3Dsizei count, WebGLId* ids) {
  for (int i = 0; i < count; ++i)
    RetireBufferId(ids[i]);
}

void TestWebGraphicsContext3D::deleteFramebuffers(
    WGC3Dsizei count, WebGLId* ids) {
  for (int i = 0; i < count; ++i)
    DCHECK_EQ(kFramebufferId | context_id_ << 16, ids[i]);
}

void TestWebGraphicsContext3D::deleteRenderbuffers(
    WGC3Dsizei count, WebGLId* ids) {
  for (int i = 0; i < count; ++i)
    DCHECK_EQ(kRenderbufferId | context_id_ << 16, ids[i]);
}

void TestWebGraphicsContext3D::deleteTextures(WGC3Dsizei count, WebGLId* ids) {
  for (int i = 0; i < count; ++i)
    RetireTextureId(ids[i]);
  base::AutoLock lock(namespace_->lock);
  for (int i = 0; i < count; ++i) {
    namespace_->textures.Remove(ids[i]);
    texture_targets_.UnbindTexture(ids[i]);
  }
}

WebGLId TestWebGraphicsContext3D::createBuffer() {
  WebGLId id;
  genBuffers(1, &id);
  return id;
}

WebGLId TestWebGraphicsContext3D::createFramebuffer() {
  WebGLId id;
  genFramebuffers(1, &id);
  return id;
}

WebGLId TestWebGraphicsContext3D::createRenderbuffer() {
  WebGLId id;
  genRenderbuffers(1, &id);
  return id;
}

WebGLId TestWebGraphicsContext3D::createTexture() {
  WebGLId id;
  genTextures(1, &id);
  return id;
}

void TestWebGraphicsContext3D::deleteBuffer(WebGLId id) {
  deleteBuffers(1, &id);
}

void TestWebGraphicsContext3D::deleteFramebuffer(WebGLId id) {
  deleteFramebuffers(1, &id);
}

void TestWebGraphicsContext3D::deleteRenderbuffer(WebGLId id) {
  deleteRenderbuffers(1, &id);
}

void TestWebGraphicsContext3D::deleteTexture(WebGLId id) {
  deleteTextures(1, &id);
}

unsigned TestWebGraphicsContext3D::createProgram() {
  unsigned program = next_program_id_++ | context_id_ << 16;
  program_set_.insert(program);
  return program;
}

WebGLId TestWebGraphicsContext3D::createShader(WGC3Denum) {
  unsigned shader = next_shader_id_++ | context_id_ << 16;
  shader_set_.insert(shader);
  return shader;
}

WebGLId TestWebGraphicsContext3D::createExternalTexture() {
  base::AutoLock lock(namespace_->lock);
  namespace_->textures.Append(kExternalTextureId, new TestTexture());
  return kExternalTextureId;
}

void TestWebGraphicsContext3D::deleteProgram(WebGLId id) {
  if (!program_set_.count(id))
    ADD_FAILURE() << "deleteProgram called on unknown program " << id;
  program_set_.erase(id);
}

void TestWebGraphicsContext3D::deleteShader(WebGLId id) {
  if (!shader_set_.count(id))
    ADD_FAILURE() << "deleteShader called on unknown shader " << id;
  shader_set_.erase(id);
}

void TestWebGraphicsContext3D::attachShader(WebGLId program, WebGLId shader) {
  if (!program_set_.count(program))
    ADD_FAILURE() << "attachShader called with unknown program " << program;
  if (!shader_set_.count(shader))
    ADD_FAILURE() << "attachShader called with unknown shader " << shader;
}

void TestWebGraphicsContext3D::useProgram(WebGLId program) {
  if (!program)
    return;
  if (!program_set_.count(program))
    ADD_FAILURE() << "useProgram called on unknown program " << program;
}

void TestWebGraphicsContext3D::bindFramebuffer(
    WGC3Denum target, WebGLId framebuffer) {
  if (!framebuffer)
    return;
  DCHECK_EQ(kFramebufferId | context_id_ << 16, framebuffer);
}

void TestWebGraphicsContext3D::bindRenderbuffer(
      WGC3Denum target, WebGLId renderbuffer) {
  if (!renderbuffer)
    return;
  DCHECK_EQ(kRenderbufferId | context_id_ << 16, renderbuffer);
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
  base::AutoLock lock(namespace_->lock);
  DCHECK(namespace_->textures.ContainsId(texture_id));
  texture_targets_.BindTexture(target, texture_id);
  used_textures_.insert(texture_id);
}

blink::WebGLId TestWebGraphicsContext3D::BoundTextureId(
    blink::WGC3Denum target) {
  return texture_targets_.BoundTexture(target);
}

scoped_refptr<TestTexture> TestWebGraphicsContext3D::BoundTexture(
    WGC3Denum target) {
  // The caller is expected to lock the namespace for texture access.
  namespace_->lock.AssertAcquired();
  return namespace_->textures.TextureForId(BoundTextureId(target));
}

void TestWebGraphicsContext3D::CheckTextureIsBound(WGC3Denum target) {
  DCHECK(BoundTextureId(target));
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
    blink::WGC3Dint* value) {
  if (pname == GL_MAX_TEXTURE_SIZE)
    *value = max_texture_size_;
  else if (pname == GL_ACTIVE_TEXTURE)
    *value = GL_TEXTURE0;
}

void TestWebGraphicsContext3D::genMailboxCHROMIUM(blink::WGC3Dbyte* mailbox) {
  if (times_gen_mailbox_succeeds_ >= 0) {
    if (!times_gen_mailbox_succeeds_) {
      loseContextCHROMIUM(GL_GUILTY_CONTEXT_RESET_ARB,
                          GL_INNOCENT_CONTEXT_RESET_ARB);
    }
    --times_gen_mailbox_succeeds_;
  }
  if (context_lost_) {
    memset(mailbox, 0, 64);
    return;
  }

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

void TestWebGraphicsContext3D::finish() {
  test_support_->CallAllSyncPointCallbacks();
}

void TestWebGraphicsContext3D::flush() {
  test_support_->CallAllSyncPointCallbacks();
}

void TestWebGraphicsContext3D::bindBuffer(blink::WGC3Denum target,
                                          blink::WebGLId buffer) {
  bound_buffer_ = buffer;
  if (!bound_buffer_)
    return;
  unsigned context_id = buffer >> 16;
  unsigned buffer_id = buffer & 0xffff;
  base::AutoLock lock(namespace_->lock);
  DCHECK(buffer_id);
  DCHECK_LT(buffer_id, namespace_->next_buffer_id);
  DCHECK_EQ(context_id, context_id_);

  base::ScopedPtrHashMap<unsigned, Buffer>& buffers = namespace_->buffers;
  if (buffers.count(bound_buffer_) == 0)
    buffers.set(bound_buffer_, make_scoped_ptr(new Buffer).Pass());

  buffers.get(bound_buffer_)->target = target;
}

void TestWebGraphicsContext3D::bufferData(blink::WGC3Denum target,
                                          blink::WGC3Dsizeiptr size,
                                          const void* data,
                                          blink::WGC3Denum usage) {
  base::AutoLock lock(namespace_->lock);
  base::ScopedPtrHashMap<unsigned, Buffer>& buffers = namespace_->buffers;
  DCHECK_GT(buffers.count(bound_buffer_), 0u);
  DCHECK_EQ(target, buffers.get(bound_buffer_)->target);
  Buffer* buffer = buffers.get(bound_buffer_);
  if (context_lost_) {
    buffer->pixels.reset();
    return;
  }

  buffer->pixels.reset(new uint8[size]);
  buffer->size = size;
  if (data != NULL)
    memcpy(buffer->pixels.get(), data, size);

  peak_transfer_buffer_memory_used_bytes_ =
      std::max(peak_transfer_buffer_memory_used_bytes_,
               GetTransferBufferMemoryUsedBytes());
}

void* TestWebGraphicsContext3D::mapBufferCHROMIUM(blink::WGC3Denum target,
                                                  blink::WGC3Denum access) {
  base::AutoLock lock(namespace_->lock);
  base::ScopedPtrHashMap<unsigned, Buffer>& buffers = namespace_->buffers;
  DCHECK_GT(buffers.count(bound_buffer_), 0u);
  DCHECK_EQ(target, buffers.get(bound_buffer_)->target);
  if (times_map_buffer_chromium_succeeds_ >= 0) {
    if (!times_map_buffer_chromium_succeeds_) {
      return NULL;
    }
    --times_map_buffer_chromium_succeeds_;
  }

  peak_transfer_buffer_memory_used_bytes_ =
      std::max(peak_transfer_buffer_memory_used_bytes_,
               GetTransferBufferMemoryUsedBytes());

  return buffers.get(bound_buffer_)->pixels.get();
}

blink::WGC3Dboolean TestWebGraphicsContext3D::unmapBufferCHROMIUM(
    blink::WGC3Denum target) {
  base::AutoLock lock(namespace_->lock);
  base::ScopedPtrHashMap<unsigned, Buffer>& buffers = namespace_->buffers;
  DCHECK_GT(buffers.count(bound_buffer_), 0u);
  DCHECK_EQ(target, buffers.get(bound_buffer_)->target);
  buffers.get(bound_buffer_)->pixels.reset();
  return true;
}

blink::WGC3Duint TestWebGraphicsContext3D::createImageCHROMIUM(
      blink::WGC3Dsizei width, blink::WGC3Dsizei height,
      blink::WGC3Denum internalformat) {
  DCHECK_EQ(GL_RGBA8_OES, static_cast<int>(internalformat));
  blink::WGC3Duint image_id = NextImageId();
  base::AutoLock lock(namespace_->lock);
  base::ScopedPtrHashMap<unsigned, Image>& images = namespace_->images;
  images.set(image_id, make_scoped_ptr(new Image).Pass());
  images.get(image_id)->pixels.reset(new uint8[width * height * 4]);
  return image_id;
}

void TestWebGraphicsContext3D::destroyImageCHROMIUM(
    blink::WGC3Duint id) {
  RetireImageId(id);
}

void TestWebGraphicsContext3D::getImageParameterivCHROMIUM(
    blink::WGC3Duint image_id,
    blink::WGC3Denum pname,
    blink::WGC3Dint* params) {
  base::AutoLock lock(namespace_->lock);
  DCHECK_GT(namespace_->images.count(image_id), 0u);
  DCHECK_EQ(GL_IMAGE_ROWBYTES_CHROMIUM, static_cast<int>(pname));
  *params = 0;
}

void* TestWebGraphicsContext3D::mapImageCHROMIUM(blink::WGC3Duint image_id,
                                                 blink::WGC3Denum access) {
  base::AutoLock lock(namespace_->lock);
  base::ScopedPtrHashMap<unsigned, Image>& images = namespace_->images;
  DCHECK_GT(images.count(image_id), 0u);
  if (times_map_image_chromium_succeeds_ >= 0) {
    if (!times_map_image_chromium_succeeds_) {
      return NULL;
    }
    --times_map_image_chromium_succeeds_;
  }
  return images.get(image_id)->pixels.get();
}

void TestWebGraphicsContext3D::unmapImageCHROMIUM(
    blink::WGC3Duint image_id) {
  base::AutoLock lock(namespace_->lock);
  DCHECK_GT(namespace_->images.count(image_id), 0u);
}

unsigned TestWebGraphicsContext3D::insertSyncPoint() {
  return next_insert_sync_point_++;
}

void TestWebGraphicsContext3D::waitSyncPoint(unsigned sync_point) {
  if (sync_point)
    last_waited_sync_point_ = sync_point;
}

size_t TestWebGraphicsContext3D::NumTextures() const {
  base::AutoLock lock(namespace_->lock);
  return namespace_->textures.Size();
}

blink::WebGLId TestWebGraphicsContext3D::TextureAt(int i) const {
  base::AutoLock lock(namespace_->lock);
  return namespace_->textures.IdAt(i);
}

WebGLId TestWebGraphicsContext3D::NextTextureId() {
  base::AutoLock lock(namespace_->lock);
  WebGLId texture_id = namespace_->next_texture_id++;
  DCHECK(texture_id < (1 << 16));
  texture_id |= context_id_ << 16;
  return texture_id;
}

void TestWebGraphicsContext3D::RetireTextureId(WebGLId id) {
  base::AutoLock lock(namespace_->lock);
  unsigned context_id = id >> 16;
  unsigned texture_id = id & 0xffff;
  DCHECK(texture_id);
  DCHECK_LT(texture_id, namespace_->next_texture_id);
  DCHECK_EQ(context_id, context_id_);
}

WebGLId TestWebGraphicsContext3D::NextBufferId() {
  base::AutoLock lock(namespace_->lock);
  WebGLId buffer_id = namespace_->next_buffer_id++;
  DCHECK(buffer_id < (1 << 16));
  buffer_id |= context_id_ << 16;
  return buffer_id;
}

void TestWebGraphicsContext3D::RetireBufferId(WebGLId id) {
  base::AutoLock lock(namespace_->lock);
  unsigned context_id = id >> 16;
  unsigned buffer_id = id & 0xffff;
  DCHECK(buffer_id);
  DCHECK_LT(buffer_id, namespace_->next_buffer_id);
  DCHECK_EQ(context_id, context_id_);
}

blink::WGC3Duint TestWebGraphicsContext3D::NextImageId() {
  base::AutoLock lock(namespace_->lock);
  WGC3Duint image_id = namespace_->next_image_id++;
  DCHECK(image_id < (1 << 16));
  image_id |= context_id_ << 16;
  return image_id;
}

void TestWebGraphicsContext3D::RetireImageId(WebGLId id) {
  base::AutoLock lock(namespace_->lock);
  unsigned context_id = id >> 16;
  unsigned image_id = id & 0xffff;
  DCHECK(image_id);
  DCHECK_LT(image_id, namespace_->next_image_id);
  DCHECK_EQ(context_id, context_id_);
}

size_t TestWebGraphicsContext3D::GetTransferBufferMemoryUsedBytes() const {
  size_t total_bytes = 0;
  base::ScopedPtrHashMap<unsigned, Buffer>& buffers = namespace_->buffers;
  base::ScopedPtrHashMap<unsigned, Buffer>::iterator it = buffers.begin();
  for (; it != buffers.end(); ++it) {
    Buffer* buffer = it->second;
    if (buffer->target == GL_PIXEL_UNPACK_TRANSFER_BUFFER_CHROMIUM)
      total_bytes += buffer->size;
  }
  return total_bytes;
}

void TestWebGraphicsContext3D::SetMaxTransferBufferUsageBytes(
    size_t max_transfer_buffer_usage_bytes) {
  test_capabilities_.max_transfer_buffer_usage_bytes =
      max_transfer_buffer_usage_bytes;
}

TestWebGraphicsContext3D::TextureTargets::TextureTargets() {
  // Initialize default bindings.
  bound_textures_[GL_TEXTURE_2D] = 0;
  bound_textures_[GL_TEXTURE_EXTERNAL_OES] = 0;
  bound_textures_[GL_TEXTURE_RECTANGLE_ARB] = 0;
}

TestWebGraphicsContext3D::TextureTargets::~TextureTargets() {}

void TestWebGraphicsContext3D::TextureTargets::BindTexture(
    blink::WGC3Denum target,
    blink::WebGLId id) {
  // Make sure this is a supported target by seeing if it was bound to before.
  DCHECK(bound_textures_.find(target) != bound_textures_.end());
  bound_textures_[target] = id;
}

void TestWebGraphicsContext3D::texParameteri(blink::WGC3Denum target,
                                             blink::WGC3Denum pname,
                                             blink::WGC3Dint param) {
  CheckTextureIsBound(target);
  base::AutoLock lock_for_texture_access(namespace_->lock);
  scoped_refptr<TestTexture> texture = BoundTexture(target);
  DCHECK(texture->IsValidParameter(pname));
  texture->params[pname] = param;
}

void TestWebGraphicsContext3D::getTexParameteriv(blink::WGC3Denum target,
                                                 blink::WGC3Denum pname,
                                                 blink::WGC3Dint* value) {
  CheckTextureIsBound(target);
  base::AutoLock lock_for_texture_access(namespace_->lock);
  scoped_refptr<TestTexture> texture = BoundTexture(target);
  DCHECK(texture->IsValidParameter(pname));
  TestTexture::TextureParametersMap::iterator it = texture->params.find(pname);
  if (it != texture->params.end())
    *value = it->second;
}

void TestWebGraphicsContext3D::TextureTargets::UnbindTexture(
    blink::WebGLId id) {
  // Bind zero to any targets that the id is bound to.
  for (TargetTextureMap::iterator it = bound_textures_.begin();
       it != bound_textures_.end();
       it++) {
    if (it->second == id)
      it->second = 0;
  }
}

blink::WebGLId TestWebGraphicsContext3D::TextureTargets::BoundTexture(
    blink::WGC3Denum target) {
  DCHECK(bound_textures_.find(target) != bound_textures_.end());
  return bound_textures_[target];
}

TestWebGraphicsContext3D::Buffer::Buffer() : target(0), size(0) {}

TestWebGraphicsContext3D::Buffer::~Buffer() {}

TestWebGraphicsContext3D::Image::Image() {}

TestWebGraphicsContext3D::Image::~Image() {}

}  // namespace cc
