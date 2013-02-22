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
#include "cc/fake_web_graphics_context_3d.h"
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
  virtual void bindBuffer(WebKit::WGC3Denum target, WebKit::WebGLId buffer);
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

  virtual void setContextLostCallback(
      WebGraphicsContextLostCallback* callback);

  virtual void loseContextCHROMIUM(WebKit::WGC3Denum current,
                                   WebKit::WGC3Denum other);

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

  static const WebKit::WebGLId kExternalTextureId;
  virtual WebKit::WebGLId NextTextureId();

 protected:
  TestWebGraphicsContext3D();
  TestWebGraphicsContext3D(
      const WebKit::WebGraphicsContext3D::Attributes& attributes);

  unsigned context_id_;
  unsigned next_texture_id_;
  Attributes attributes_;
  bool have_extension_io_surface_;
  bool have_extension_egl_image_;
  int times_make_current_succeeds_;
  int times_bind_texture_succeeds_;
  int times_end_query_succeeds_;
  bool context_lost_;
  WebGraphicsContextLostCallback* context_lost_callback_;
  std::vector<WebKit::WebGLId> textures_;
  base::hash_set<WebKit::WebGLId> used_textures_;
  std::vector<WebKit::WebGraphicsContext3D*> shared_contexts_;
  int width_;
  int height_;
};

}  // namespace cc

#endif  // CC_TEST_TEST_WEB_GRAPHICS_CONTEXT_3D_H_
