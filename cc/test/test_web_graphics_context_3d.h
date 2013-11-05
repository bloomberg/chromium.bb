// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_TEST_WEB_GRAPHICS_CONTEXT_3D_H_
#define CC_TEST_TEST_WEB_GRAPHICS_CONTEXT_3D_H_

#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/hash_tables.h"
#include "base/containers/scoped_ptr_hash_map.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/stl_util.h"
#include "base/synchronization/lock.h"
#include "cc/output/context_provider.h"
#include "cc/test/fake_web_graphics_context_3d.h"
#include "cc/test/ordered_texture_map.h"
#include "cc/test/test_texture.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "ui/gfx/rect.h"

namespace cc {
class TestContextSupport;

class TestWebGraphicsContext3D : public FakeWebGraphicsContext3D {
 public:
  static scoped_ptr<TestWebGraphicsContext3D> Create();

  virtual ~TestWebGraphicsContext3D();

  virtual bool makeContextCurrent();

  virtual void reshapeWithScaleFactor(
      int width, int height, float scale_factor);

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

  virtual void genBuffers(WebKit::WGC3Dsizei count, WebKit::WebGLId* ids);
  virtual void genFramebuffers(WebKit::WGC3Dsizei count, WebKit::WebGLId* ids);
  virtual void genRenderbuffers(WebKit::WGC3Dsizei count, WebKit::WebGLId* ids);
  virtual void genTextures(WebKit::WGC3Dsizei count, WebKit::WebGLId* ids);

  virtual void deleteBuffers(WebKit::WGC3Dsizei count, WebKit::WebGLId* ids);
  virtual void deleteFramebuffers(
      WebKit::WGC3Dsizei count, WebKit::WebGLId* ids);
  virtual void deleteRenderbuffers(
      WebKit::WGC3Dsizei count, WebKit::WebGLId* ids);
  virtual void deleteTextures(WebKit::WGC3Dsizei count, WebKit::WebGLId* ids);

  virtual WebKit::WebGLId createBuffer();
  virtual WebKit::WebGLId createFramebuffer();
  virtual WebKit::WebGLId createRenderbuffer();
  virtual WebKit::WebGLId createTexture();

  virtual void deleteBuffer(WebKit::WebGLId id);
  virtual void deleteFramebuffer(WebKit::WebGLId id);
  virtual void deleteRenderbuffer(WebKit::WebGLId id);
  virtual void deleteTexture(WebKit::WebGLId id);

  virtual WebKit::WebGLId createProgram();
  virtual WebKit::WebGLId createShader(WebKit::WGC3Denum);
  virtual WebKit::WebGLId createExternalTexture();

  virtual void deleteProgram(WebKit::WebGLId id);
  virtual void deleteShader(WebKit::WebGLId id);

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

  virtual void setSwapBuffersCompleteCallbackCHROMIUM(
      WebGraphicsSwapBuffersCompleteCallbackCHROMIUM* callback);

  virtual void prepareTexture();
  virtual void postSubBufferCHROMIUM(int x, int y, int width, int height);
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

  virtual WebKit::WGC3Duint createImageCHROMIUM(
      WebKit::WGC3Dsizei width,
      WebKit::WGC3Dsizei height,
      WebKit::WGC3Denum internalformat);
  virtual void destroyImageCHROMIUM(WebKit::WGC3Duint image_id);
  virtual void getImageParameterivCHROMIUM(
      WebKit::WGC3Duint image_id,
      WebKit::WGC3Denum pname,
      WebKit::WGC3Dint* params);
  virtual void* mapImageCHROMIUM(
      WebKit::WGC3Duint image_id,
      WebKit::WGC3Denum access);
  virtual void unmapImageCHROMIUM(WebKit::WGC3Duint image_id);

  const ContextProvider::Capabilities& test_capabilities() const {
    return test_capabilities_;
  }

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
  void set_times_gen_mailbox_succeeds(int times) {
    times_gen_mailbox_succeeds_ = times;
  }

  // When set, mapImageCHROMIUM and mapBufferCHROMIUM will return NULL after
  // this many times.
  void set_times_map_image_chromium_succeeds(int times) {
    times_map_image_chromium_succeeds_ = times;
  }
  void set_times_map_buffer_chromium_succeeds(int times) {
    times_map_buffer_chromium_succeeds_ = times;
  }

  size_t NumTextures() const;
  WebKit::WebGLId TextureAt(int i) const;

  size_t NumUsedTextures() const { return used_textures_.size(); }
  bool UsedTexture(int texture) const {
    return ContainsKey(used_textures_, texture);
  }
  void ResetUsedTextures() { used_textures_.clear(); }

  void set_support_swapbuffers_complete_callback(bool support) {
    test_capabilities_.swapbuffers_complete_callback = support;
  }
  void set_have_extension_io_surface(bool have) {
    test_capabilities_.iosurface = have;
    test_capabilities_.texture_rectangle = have;
  }
  void set_have_extension_egl_image(bool have) {
    test_capabilities_.egl_image_external = have;
  }
  void set_have_post_sub_buffer(bool have) {
    test_capabilities_.post_sub_buffer = have;
  }
  void set_have_discard_framebuffer(bool have) {
    test_capabilities_.discard_framebuffer = have;
  }
  void set_support_compressed_texture_etc1(bool support) {
    test_capabilities_.texture_format_etc1 = support;
  }

  // When this context is lost, all contexts in its share group are also lost.
  void add_share_group_context(WebKit::WebGraphicsContext3D* context3d) {
    shared_contexts_.push_back(context3d);
  }

  void set_max_texture_size(int size) { max_texture_size_ = size; }

  static const WebKit::WebGLId kExternalTextureId;
  virtual WebKit::WebGLId NextTextureId();
  virtual void RetireTextureId(WebKit::WebGLId id);

  virtual WebKit::WebGLId NextBufferId();
  virtual void RetireBufferId(WebKit::WebGLId id);

  virtual WebKit::WebGLId NextImageId();
  virtual void RetireImageId(WebKit::WebGLId id);

  size_t GetTransferBufferMemoryUsedBytes() const;
  void SetMaxTransferBufferUsageBytes(size_t max_transfer_buffer_usage_bytes);

  void set_test_support(TestContextSupport* test_support) {
    test_support_ = test_support;
  }

  int width() const { return width_; }
  int height() const { return height_; }
  bool reshape_called() const { return reshape_called_; }
  void clear_reshape_called() { reshape_called_ = false; }
  float scale_factor() const { return scale_factor_; }

  enum UpdateType {
    NoUpdate = 0,
    PrepareTexture,
    PostSubBuffer
  };

  gfx::Rect update_rect() const { return update_rect_; }

  UpdateType last_update_type() {
    return last_update_type_;
  }

 protected:
  struct TextureTargets {
    TextureTargets();
    ~TextureTargets();

    void BindTexture(WebKit::WGC3Denum target, WebKit::WebGLId id);
    void UnbindTexture(WebKit::WebGLId id);

    WebKit::WebGLId BoundTexture(WebKit::WGC3Denum target);

   private:
    typedef base::hash_map<WebKit::WGC3Denum, WebKit::WebGLId> TargetTextureMap;
    TargetTextureMap bound_textures_;
  };

  struct Buffer {
    Buffer();
    ~Buffer();

    WebKit::WGC3Denum target;
    scoped_ptr<uint8[]> pixels;
    size_t size;

   private:
    DISALLOW_COPY_AND_ASSIGN(Buffer);
  };

  struct Image {
    Image();
    ~Image();

    scoped_ptr<uint8[]> pixels;

   private:
    DISALLOW_COPY_AND_ASSIGN(Image);
  };

  struct Namespace : public base::RefCountedThreadSafe<Namespace> {
    Namespace();

    // Protects all fields.
    base::Lock lock;
    unsigned next_buffer_id;
    unsigned next_image_id;
    unsigned next_texture_id;
    base::ScopedPtrHashMap<unsigned, Buffer> buffers;
    base::ScopedPtrHashMap<unsigned, Image> images;
    OrderedTextureMap textures;

   private:
    friend class base::RefCountedThreadSafe<Namespace>;
    ~Namespace();
    DISALLOW_COPY_AND_ASSIGN(Namespace);
  };

  TestWebGraphicsContext3D();

  void CallAllSyncPointCallbacks();
  void SwapBuffersComplete();
  void CreateNamespace();
  WebKit::WebGLId BoundTextureId(WebKit::WGC3Denum target);

  unsigned context_id_;
  Attributes attributes_;
  ContextProvider::Capabilities test_capabilities_;
  int times_make_current_succeeds_;
  int times_bind_texture_succeeds_;
  int times_end_query_succeeds_;
  int times_gen_mailbox_succeeds_;
  bool context_lost_;
  int times_map_image_chromium_succeeds_;
  int times_map_buffer_chromium_succeeds_;
  WebGraphicsContextLostCallback* context_lost_callback_;
  WebGraphicsSwapBuffersCompleteCallbackCHROMIUM* swap_buffers_callback_;
  base::hash_set<unsigned> used_textures_;
  unsigned next_program_id_;
  base::hash_set<unsigned> program_set_;
  unsigned next_shader_id_;
  base::hash_set<unsigned> shader_set_;
  std::vector<WebKit::WebGraphicsContext3D*> shared_contexts_;
  int max_texture_size_;
  bool reshape_called_;
  int width_;
  int height_;
  float scale_factor_;
  TestContextSupport* test_support_;
  gfx::Rect update_rect_;
  UpdateType last_update_type_;

  unsigned bound_buffer_;
  TextureTargets texture_targets_;

  scoped_refptr<Namespace> namespace_;
  static Namespace* shared_namespace_;

  base::WeakPtrFactory<TestWebGraphicsContext3D> weak_ptr_factory_;
};

}  // namespace cc

#endif  // CC_TEST_TEST_WEB_GRAPHICS_CONTEXT_3D_H_
