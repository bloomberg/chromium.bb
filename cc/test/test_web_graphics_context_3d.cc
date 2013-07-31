// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/test_web_graphics_context_3d.h"

#include <algorithm>
#include <string>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
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

TestWebGraphicsContext3D::TestWebGraphicsContext3D()
    : FakeWebGraphicsContext3D(),
      context_id_(s_context_id++),
      support_swapbuffers_complete_callback_(true),
      have_extension_io_surface_(false),
      have_extension_egl_image_(false),
      times_make_current_succeeds_(-1),
      times_bind_texture_succeeds_(-1),
      times_end_query_succeeds_(-1),
      times_gen_mailbox_succeeds_(-1),
      context_lost_(false),
      times_map_image_chromium_succeeds_(-1),
      times_map_buffer_chromium_succeeds_(-1),
      context_lost_callback_(NULL),
      swap_buffers_callback_(NULL),
      memory_allocation_changed_callback_(NULL),
      max_texture_size_(1024),
      width_(0),
      height_(0),
      bound_buffer_(0),
      weak_ptr_factory_(this) {
  CreateNamespace();
}

TestWebGraphicsContext3D::TestWebGraphicsContext3D(
    const WebGraphicsContext3D::Attributes& attributes)
    : FakeWebGraphicsContext3D(),
      context_id_(s_context_id++),
      attributes_(attributes),
      support_swapbuffers_complete_callback_(true),
      have_extension_io_surface_(false),
      have_extension_egl_image_(false),
      times_make_current_succeeds_(-1),
      times_bind_texture_succeeds_(-1),
      times_end_query_succeeds_(-1),
      times_gen_mailbox_succeeds_(-1),
      context_lost_(false),
      times_map_image_chromium_succeeds_(-1),
      times_map_buffer_chromium_succeeds_(-1),
      context_lost_callback_(NULL),
      swap_buffers_callback_(NULL),
      memory_allocation_changed_callback_(NULL),
      max_texture_size_(1024),
      width_(0),
      height_(0),
      bound_buffer_(0),
      weak_ptr_factory_(this) {
  CreateNamespace();
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

TestWebGraphicsContext3D::~TestWebGraphicsContext3D() {
  for (size_t i = 0; i < sync_point_callbacks_.size(); ++i) {
    if (sync_point_callbacks_[i] != NULL)
      delete sync_point_callbacks_[i];
  }
  base::AutoLock lock(g_shared_namespace_lock.Get());
  namespace_ = NULL;
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

void TestWebGraphicsContext3D::reshapeWithScaleFactor(
    int width, int height, float scale_factor) {
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

  if (support_swapbuffers_complete_callback_)
    string += "GL_CHROMIUM_swapbuffers_complete_callback";

  if (name == GL_EXTENSIONS) {
    if (have_extension_io_surface_)
      string += " GL_CHROMIUM_iosurface GL_ARB_texture_rectangle";
    if (have_extension_egl_image_)
      string += " GL_OES_EGL_image_external";
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
  base::AutoLock lock(namespace_->lock);
  unsigned context_id = id >> 17;
  unsigned buffer_id = id & 0x1ffff;
  DCHECK(buffer_id && buffer_id < namespace_->next_buffer_id);
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
  base::AutoLock lock(namespace_->lock);
  namespace_->textures.push_back(texture_id);
  return texture_id;
}

void TestWebGraphicsContext3D::deleteTexture(WebGLId texture_id) {
  base::AutoLock lock(namespace_->lock);
  std::vector<WebKit::WebGLId>& textures = namespace_->textures;
  DCHECK(std::find(textures.begin(), textures.end(), texture_id) !=
         textures.end());
  textures.erase(std::find(textures.begin(), textures.end(), texture_id));
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
  base::AutoLock lock(namespace_->lock);
  std::vector<WebKit::WebGLId>& textures = namespace_->textures;
  DCHECK(std::find(textures.begin(), textures.end(), texture_id) !=
         textures.end());
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
  else if (pname == GL_ACTIVE_TEXTURE)
    *value = GL_TEXTURE0;
}

void TestWebGraphicsContext3D::genMailboxCHROMIUM(WebKit::WGC3Dbyte* mailbox) {
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

void TestWebGraphicsContext3D::signalSyncPoint(
    unsigned sync_point,
    WebGraphicsSyncPointCallback* callback) {
  sync_point_callbacks_.push_back(callback);
}

void TestWebGraphicsContext3D::signalQuery(
    WebKit::WebGLId query,
    WebGraphicsSyncPointCallback* callback) {
  sync_point_callbacks_.push_back(callback);
}

void TestWebGraphicsContext3D::setSwapBuffersCompleteCallbackCHROMIUM(
    WebGraphicsSwapBuffersCompleteCallbackCHROMIUM* callback) {
  if (support_swapbuffers_complete_callback_)
    swap_buffers_callback_ = callback;
}

void TestWebGraphicsContext3D::setMemoryAllocationChangedCallbackCHROMIUM(
    WebGraphicsMemoryAllocationChangedCallbackCHROMIUM* callback) {
  memory_allocation_changed_callback_ = callback;
}

void TestWebGraphicsContext3D::prepareTexture() {
  if (swap_buffers_callback_) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(&TestWebGraphicsContext3D::SwapBuffersComplete,
                              weak_ptr_factory_.GetWeakPtr()));
  }
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

void TestWebGraphicsContext3D::SwapBuffersComplete() {
  if (swap_buffers_callback_)
    swap_buffers_callback_->onSwapBuffersComplete();
}

void TestWebGraphicsContext3D::bindBuffer(WebKit::WGC3Denum target,
                                          WebKit::WebGLId buffer) {
  bound_buffer_ = buffer;
  if (!bound_buffer_)
    return;
  unsigned context_id = buffer >> 17;
  unsigned buffer_id = buffer & 0x1ffff;
  base::AutoLock lock(namespace_->lock);
  DCHECK(buffer_id && buffer_id < namespace_->next_buffer_id);
  DCHECK_EQ(context_id, context_id_);

  ScopedPtrHashMap<unsigned, Buffer>& buffers = namespace_->buffers;
  if (buffers.count(bound_buffer_) == 0)
    buffers.set(bound_buffer_, make_scoped_ptr(new Buffer).Pass());

  buffers.get(bound_buffer_)->target = target;
}

void TestWebGraphicsContext3D::bufferData(WebKit::WGC3Denum target,
                                          WebKit::WGC3Dsizeiptr size,
                                          const void* data,
                                          WebKit::WGC3Denum usage) {
  base::AutoLock lock(namespace_->lock);
  ScopedPtrHashMap<unsigned, Buffer>& buffers = namespace_->buffers;
  DCHECK_GT(buffers.count(bound_buffer_), 0u);
  DCHECK_EQ(target, buffers.get(bound_buffer_)->target);
  if (context_lost_) {
    buffers.get(bound_buffer_)->pixels.reset();
    return;
  }
  buffers.get(bound_buffer_)->pixels.reset(new uint8[size]);
  if (data != NULL)
    memcpy(buffers.get(bound_buffer_)->pixels.get(), data, size);
}

void* TestWebGraphicsContext3D::mapBufferCHROMIUM(WebKit::WGC3Denum target,
                                                  WebKit::WGC3Denum access) {
  base::AutoLock lock(namespace_->lock);
  ScopedPtrHashMap<unsigned, Buffer>& buffers = namespace_->buffers;
  DCHECK_GT(buffers.count(bound_buffer_), 0u);
  DCHECK_EQ(target, buffers.get(bound_buffer_)->target);
  if (times_map_buffer_chromium_succeeds_ >= 0) {
    if (!times_map_buffer_chromium_succeeds_) {
      return NULL;
    }
    --times_map_buffer_chromium_succeeds_;
  }
  return buffers.get(bound_buffer_)->pixels.get();
}

WebKit::WGC3Dboolean TestWebGraphicsContext3D::unmapBufferCHROMIUM(
    WebKit::WGC3Denum target) {
  base::AutoLock lock(namespace_->lock);
  ScopedPtrHashMap<unsigned, Buffer>& buffers = namespace_->buffers;
  DCHECK_GT(buffers.count(bound_buffer_), 0u);
  DCHECK_EQ(target, buffers.get(bound_buffer_)->target);
  buffers.get(bound_buffer_)->pixels.reset();
  return true;
}

void TestWebGraphicsContext3D::bindTexImage2DCHROMIUM(
    WebKit::WGC3Denum target,
    WebKit::WGC3Dint image_id) {
  base::AutoLock lock(namespace_->lock);
  DCHECK_GT(namespace_->images.count(image_id), 0u);
}

WebKit::WGC3Duint TestWebGraphicsContext3D::createImageCHROMIUM(
      WebKit::WGC3Dsizei width, WebKit::WGC3Dsizei height,
      WebKit::WGC3Denum internalformat) {
  DCHECK_EQ(GL_RGBA8_OES, static_cast<int>(internalformat));
  WebKit::WGC3Duint image_id = NextImageId();
  base::AutoLock lock(namespace_->lock);
  ScopedPtrHashMap<unsigned, Image>& images = namespace_->images;
  images.set(image_id, make_scoped_ptr(new Image).Pass());
  images.get(image_id)->pixels.reset(new uint8[width * height * 4]);
  return image_id;
}

void TestWebGraphicsContext3D::destroyImageCHROMIUM(
    WebKit::WGC3Duint id) {
  base::AutoLock lock(namespace_->lock);
  unsigned context_id = id >> 17;
  unsigned image_id = id & 0x1ffff;
  DCHECK(image_id && image_id < namespace_->next_image_id);
  DCHECK_EQ(context_id, context_id_);
}

void TestWebGraphicsContext3D::getImageParameterivCHROMIUM(
    WebKit::WGC3Duint image_id,
    WebKit::WGC3Denum pname,
    WebKit::WGC3Dint* params) {
  base::AutoLock lock(namespace_->lock);
  DCHECK_GT(namespace_->images.count(image_id), 0u);
  DCHECK_EQ(GL_IMAGE_ROWBYTES_CHROMIUM, static_cast<int>(pname));
  *params = 0;
}

void* TestWebGraphicsContext3D::mapImageCHROMIUM(WebKit::WGC3Duint image_id,
                                                 WebKit::WGC3Denum access) {
  base::AutoLock lock(namespace_->lock);
  ScopedPtrHashMap<unsigned, Image>& images = namespace_->images;
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
    WebKit::WGC3Duint image_id) {
  base::AutoLock lock(namespace_->lock);
  DCHECK_GT(namespace_->images.count(image_id), 0u);
}

size_t TestWebGraphicsContext3D::NumTextures() const {
  base::AutoLock lock(namespace_->lock);
  return namespace_->textures.size();
}

WebKit::WebGLId TestWebGraphicsContext3D::TextureAt(int i) const {
  base::AutoLock lock(namespace_->lock);
  return namespace_->textures[i];
}

WebGLId TestWebGraphicsContext3D::NextTextureId() {
  base::AutoLock lock(namespace_->lock);
  WebGLId texture_id = namespace_->next_texture_id++;
  DCHECK(texture_id < (1 << 16));
  texture_id |= context_id_ << 16;
  return texture_id;
}

WebGLId TestWebGraphicsContext3D::NextBufferId() {
  base::AutoLock lock(namespace_->lock);
  WebGLId buffer_id = namespace_->next_buffer_id++;
  DCHECK(buffer_id < (1 << 17));
  buffer_id |= context_id_ << 17;
  return buffer_id;
}

WebKit::WGC3Duint TestWebGraphicsContext3D::NextImageId() {
  base::AutoLock lock(namespace_->lock);
  WGC3Duint image_id = namespace_->next_image_id++;
  DCHECK(image_id < (1 << 17));
  image_id |= context_id_ << 17;
  return image_id;
}

void TestWebGraphicsContext3D::SetMemoryAllocation(
    WebKit::WebGraphicsMemoryAllocation allocation) {
  if (!memory_allocation_changed_callback_)
    return;
  memory_allocation_changed_callback_->onMemoryAllocationChanged(allocation);
}

TestWebGraphicsContext3D::Buffer::Buffer() : target(0) {}

TestWebGraphicsContext3D::Buffer::~Buffer() {}

TestWebGraphicsContext3D::Image::Image() {}

TestWebGraphicsContext3D::Image::~Image() {}

}  // namespace cc
