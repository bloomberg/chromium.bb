// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_TEST_WEB_GRAPHICS_CONTEXT_3D_H_
#define CC_TEST_TEST_WEB_GRAPHICS_CONTEXT_3D_H_

#include <vector>

#include "base/compiler_specific.h"
#include "base/hash_tables.h"
#include "base/memory/scoped_ptr.h"
#include "base/stl_util.h"
#include "cc/base/scoped_ptr_hash_map.h"
#include "cc/debug/fake_web_graphics_context_3d.h"
#include "third_party/khronos/GLES2/gl2.h"

namespace cc {

class TestWebGraphicsContext3D : public FakeWebGraphicsContext3D {
 public:
  static scoped_ptr<TestWebGraphicsContext3D> Create() {
    return make_scoped_ptr(new TestWebGraphicsContext3D());
  }
  static scoped_ptr<TestWebGraphicsContext3D> Create(
      const WebKit::WebGraphicsContext3D::Attributes& attributes) {
    return make_scoped_ptr(new TestWebGraphicsContext3D(attributes));
  }
  virtual ~TestWebGraphicsContext3D();

  virtual bool makeContextCurrent();

  virtual int width();
  virtual int height();

  virtual void reshape(int width, int height);

  virtual bool isContextLost();
  virtual WebKit::WGC3Denum getGraphicsResetStatusARB();

  virtual void attachShader(WebKit::WebGLId program, WebKit::WebGLId shader);
  virtual void bindFramebuffer(
      WebKit::WGC3Denum target, WebKit::WebGLId framebuffer);
  virtual void bindRenderbuffer(
      WebKit::WGC3Denum target, WebKit::WebGLId renderbuffer);
  virtual void bindTexture(
      WebKit::WGC3Denum target,
      WebKit::WebGLId texture_id);

  virtual WebKit::WGC3Denum checkFramebufferStatus(WebKit::WGC3Denum target);

  virtual Attributes getContextAttributes();

  virtual WebKit::WebString getString(WebKit::WGC3Denum name);
  virtual WebKit::WGC3Dint getUniformLocation(
      WebKit::WebGLId program,
      const WebKit::WGC3Dchar* name);
  virtual WebKit::WGC3Dsizeiptr getVertexAttribOffset(
      WebKit::WGC3Duint index,
      WebKit::WGC3Denum pname);

  virtual WebKit::WGC3Dboolean isBuffer(WebKit::WebGLId buffer);
  virtual WebKit::WGC3Dboolean isEnabled(WebKit::WGC3Denum cap);
  virtual WebKit::WGC3Dboolean isFramebuffer(WebKit::WebGLId framebuffer);
  virtual WebKit::WGC3Dboolean isProgram(WebKit::WebGLId program);
  virtual WebKit::WGC3Dboolean isRenderbuffer(WebKit::WebGLId renderbuffer);
  virtual WebKit::WGC3Dboolean isShader(WebKit::WebGLId shader);
  virtual WebKit::WGC3Dboolean isTexture(WebKit::WebGLId texture);

  virtual void useProgram(WebKit::WebGLId program);

  virtual WebKit::WebGLId createBuffer();
  virtual WebKit::WebGLId createFramebuffer();
  virtual WebKit::WebGLId createProgram();
  virtual WebKit::WebGLId createRenderbuffer();
  virtual WebKit::WebGLId createShader(WebKit::WGC3Denum);
  virtual WebKit::WebGLId createTexture();

  virtual void deleteBuffer(WebKit::WebGLId id);
  virtual void deleteFramebuffer(WebKit::WebGLId id);
  virtual void deleteProgram(WebKit::WebGLId id);
  virtual void deleteRenderbuffer(WebKit::WebGLId id);
  virtual void deleteShader(WebKit::WebGLId id);
  virtual void deleteTexture(WebKit::WebGLId texture_id);

  virtual void endQueryEXT(WebKit::WGC3Denum target);
  virtual void getQueryObjectuivEXT(
      WebKit::WebGLId query,
      WebKit::WGC3Denum pname,
      WebKit::WGC3Duint* params);

  virtual void getIntegerv(
      WebKit::WGC3Denum pname,
      WebKit::WGC3Dint* value);

  virtual void genMailboxCHROMIUM(WebKit::WGC3Dbyte* mailbox);
  virtual void produceTextureCHROMIUM(WebKit::WGC3Denum target,
                                      const WebKit::WGC3Dbyte* mailbox) { }
  virtual void consumeTextureCHROMIUM(WebKit::WGC3Denum target,
                                      const WebKit::WGC3Dbyte* mailbox) { }

  virtual void setContextLostCallback(
      WebGraphicsContextLostCallback* callback);

  virtual void loseContextCHROMIUM(WebKit::WGC3Denum current,
                                   WebKit::WGC3Denum other);

  // Takes ownership of the |callback|.
  virtual void signalSyncPoint(unsigned sync_point,
                               WebGraphicsSyncPointCallback* callback);

  virtual void prepareTexture();
  virtual void finish();
  virtual void flush();

  virtual void bindBuffer(WebKit::WGC3Denum target, WebKit::WebGLId buffer);
  virtual void bufferData(WebKit::WGC3Denum target,
                          WebKit::WGC3Dsizeiptr size,
                          const void* data,
                          WebKit::WGC3Denum usage);
  virtual void* mapBufferCHROMIUM(WebKit::WGC3Denum target,
                                  WebKit::WGC3Denum access);
  virtual WebKit::WGC3Dboolean unmapBufferCHROMIUM(WebKit::WGC3Denum target);

  // When set, MakeCurrent() will fail after this many times.
  void set_times_make_current_succeeds(int times) {
    times_make_current_succeeds_ = times;
  }
  void set_times_bind_texture_succeeds(int times) {
    times_bind_texture_succeeds_ = times;
  }
  void set_times_end_query_succeeds(int times) {
    times_end_query_succeeds_ = times;
  }

  size_t NumTextures() const { return textures_.size(); }
  WebKit::WebGLId TextureAt(int i) const { return textures_[i]; }

  size_t NumUsedTextures() const { return used_textures_.size(); }
  bool UsedTexture(int texture) const {
    return ContainsKey(used_textures_, texture);
  }
  void ResetUsedTextures() { used_textures_.clear(); }

  void set_have_extension_io_surface(bool have) {
    have_extension_io_surface_ = have;
  }
  void set_have_extension_egl_image(bool have) {
    have_extension_egl_image_ = have;
  }

  // When this context is lost, all contexts in its share group are also lost.
  void add_share_group_context(WebKit::WebGraphicsContext3D* context3d) {
    shared_contexts_.push_back(context3d);
  }

  void set_max_texture_size(int size) { max_texture_size_ = size; }

  static const WebKit::WebGLId kExternalTextureId;
  virtual WebKit::WebGLId NextTextureId();

  virtual WebKit::WebGLId NextBufferId();

 protected:
  TestWebGraphicsContext3D();
  TestWebGraphicsContext3D(
      const WebKit::WebGraphicsContext3D::Attributes& attributes);

  void CallAllSyncPointCallbacks();

  unsigned context_id_;
  unsigned next_buffer_id_;
  unsigned next_texture_id_;
  Attributes attributes_;
  bool have_extension_io_surface_;
  bool have_extension_egl_image_;
  int times_make_current_succeeds_;
  int times_bind_texture_succeeds_;
  int times_end_query_succeeds_;
  bool context_lost_;
  WebGraphicsContextLostCallback* context_lost_callback_;
  std::vector<WebGraphicsSyncPointCallback*> sync_point_callbacks_;
  std::vector<WebKit::WebGLId> textures_;
  base::hash_set<WebKit::WebGLId> used_textures_;
  std::vector<WebKit::WebGraphicsContext3D*> shared_contexts_;
  int max_texture_size_;
  int width_;
  int height_;

  struct Buffer {
    Buffer();
    ~Buffer();

    WebKit::WGC3Denum target;
    scoped_ptr<uint8[]> pixels;

   private:
    DISALLOW_COPY_AND_ASSIGN(Buffer);
  };
  ScopedPtrHashMap<unsigned, Buffer> buffers_;
  unsigned bound_buffer_;
};

}  // namespace cc

#endif  // CC_TEST_TEST_WEB_GRAPHICS_CONTEXT_3D_H_
