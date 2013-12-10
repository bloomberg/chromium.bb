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

  virtual void reshapeWithScaleFactor(
      int width, int height, float scale_factor);

  virtual bool isContextLost();

  virtual void attachShader(blink::WebGLId program, blink::WebGLId shader);
  virtual void bindFramebuffer(
      blink::WGC3Denum target, blink::WebGLId framebuffer);
  virtual void bindRenderbuffer(
      blink::WGC3Denum target, blink::WebGLId renderbuffer);
  virtual void bindTexture(
      blink::WGC3Denum target,
      blink::WebGLId texture_id);

  virtual void texParameteri(blink::WGC3Denum target,
                             blink::WGC3Denum pname,
                             blink::WGC3Dint param);
  virtual void getTexParameteriv(blink::WGC3Denum target,
                                 blink::WGC3Denum pname,
                                 blink::WGC3Dint* value);

  virtual blink::WGC3Denum checkFramebufferStatus(blink::WGC3Denum target);

  virtual Attributes getContextAttributes();

  virtual blink::WebString getString(blink::WGC3Denum name);
  virtual blink::WGC3Dint getUniformLocation(
      blink::WebGLId program,
      const blink::WGC3Dchar* name);
  virtual blink::WGC3Dsizeiptr getVertexAttribOffset(
      blink::WGC3Duint index,
      blink::WGC3Denum pname);

  virtual blink::WGC3Dboolean isBuffer(blink::WebGLId buffer);
  virtual blink::WGC3Dboolean isEnabled(blink::WGC3Denum cap);
  virtual blink::WGC3Dboolean isFramebuffer(blink::WebGLId framebuffer);
  virtual blink::WGC3Dboolean isProgram(blink::WebGLId program);
  virtual blink::WGC3Dboolean isRenderbuffer(blink::WebGLId renderbuffer);
  virtual blink::WGC3Dboolean isShader(blink::WebGLId shader);
  virtual blink::WGC3Dboolean isTexture(blink::WebGLId texture);

  virtual void useProgram(blink::WebGLId program);

  virtual void genBuffers(blink::WGC3Dsizei count, blink::WebGLId* ids);
  virtual void genFramebuffers(blink::WGC3Dsizei count, blink::WebGLId* ids);
  virtual void genRenderbuffers(blink::WGC3Dsizei count, blink::WebGLId* ids);
  virtual void genTextures(blink::WGC3Dsizei count, blink::WebGLId* ids);

  virtual void deleteBuffers(blink::WGC3Dsizei count, blink::WebGLId* ids);
  virtual void deleteFramebuffers(
      blink::WGC3Dsizei count, blink::WebGLId* ids);
  virtual void deleteRenderbuffers(
      blink::WGC3Dsizei count, blink::WebGLId* ids);
  virtual void deleteTextures(blink::WGC3Dsizei count, blink::WebGLId* ids);

  virtual blink::WebGLId createBuffer();
  virtual blink::WebGLId createFramebuffer();
  virtual blink::WebGLId createRenderbuffer();
  virtual blink::WebGLId createTexture();

  virtual void deleteBuffer(blink::WebGLId id);
  virtual void deleteFramebuffer(blink::WebGLId id);
  virtual void deleteRenderbuffer(blink::WebGLId id);
  virtual void deleteTexture(blink::WebGLId id);

  virtual blink::WebGLId createProgram();
  virtual blink::WebGLId createShader(blink::WGC3Denum);
  virtual blink::WebGLId createExternalTexture();

  virtual void deleteProgram(blink::WebGLId id);
  virtual void deleteShader(blink::WebGLId id);

  virtual void endQueryEXT(blink::WGC3Denum target);
  virtual void getQueryObjectuivEXT(
      blink::WebGLId query,
      blink::WGC3Denum pname,
      blink::WGC3Duint* params);

  virtual void getIntegerv(
      blink::WGC3Denum pname,
      blink::WGC3Dint* value);

  virtual void genMailboxCHROMIUM(blink::WGC3Dbyte* mailbox);
  virtual void produceTextureCHROMIUM(blink::WGC3Denum target,
                                      const blink::WGC3Dbyte* mailbox) { }
  virtual void consumeTextureCHROMIUM(blink::WGC3Denum target,
                                      const blink::WGC3Dbyte* mailbox) { }

  virtual void setContextLostCallback(
      WebGraphicsContextLostCallback* callback);

  virtual void loseContextCHROMIUM(blink::WGC3Denum current,
                                   blink::WGC3Denum other);

  virtual void finish();
  virtual void flush();

  virtual void bindBuffer(blink::WGC3Denum target, blink::WebGLId buffer);
  virtual void bufferData(blink::WGC3Denum target,
                          blink::WGC3Dsizeiptr size,
                          const void* data,
                          blink::WGC3Denum usage);
  virtual void* mapBufferCHROMIUM(blink::WGC3Denum target,
                                  blink::WGC3Denum access);
  virtual blink::WGC3Dboolean unmapBufferCHROMIUM(blink::WGC3Denum target);

  virtual blink::WGC3Duint createImageCHROMIUM(
      blink::WGC3Dsizei width,
      blink::WGC3Dsizei height,
      blink::WGC3Denum internalformat);
  virtual void destroyImageCHROMIUM(blink::WGC3Duint image_id);
  virtual void getImageParameterivCHROMIUM(
      blink::WGC3Duint image_id,
      blink::WGC3Denum pname,
      blink::WGC3Dint* params);
  virtual void* mapImageCHROMIUM(
      blink::WGC3Duint image_id,
      blink::WGC3Denum access);
  virtual void unmapImageCHROMIUM(blink::WGC3Duint image_id);

  virtual unsigned insertSyncPoint() OVERRIDE;
  virtual void waitSyncPoint(unsigned sync_point) OVERRIDE;

  unsigned last_waited_sync_point() const { return last_waited_sync_point_; }

  const ContextProvider::Capabilities& test_capabilities() const {
    return test_capabilities_;
  }

  void set_context_lost(bool context_lost) { context_lost_ = context_lost; }
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
  blink::WebGLId TextureAt(int i) const;

  size_t NumUsedTextures() const { return used_textures_.size(); }
  bool UsedTexture(int texture) const {
    return ContainsKey(used_textures_, texture);
  }
  void ResetUsedTextures() { used_textures_.clear(); }

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
  void set_support_texture_storage(bool support) {
    test_capabilities_.texture_storage = support;
  }

  // When this context is lost, all contexts in its share group are also lost.
  void add_share_group_context(blink::WebGraphicsContext3D* context3d) {
    shared_contexts_.push_back(context3d);
  }

  void set_max_texture_size(int size) { max_texture_size_ = size; }

  static const blink::WebGLId kExternalTextureId;
  virtual blink::WebGLId NextTextureId();
  virtual void RetireTextureId(blink::WebGLId id);

  virtual blink::WebGLId NextBufferId();
  virtual void RetireBufferId(blink::WebGLId id);

  virtual blink::WebGLId NextImageId();
  virtual void RetireImageId(blink::WebGLId id);

  size_t GetTransferBufferMemoryUsedBytes() const;
  void SetMaxTransferBufferUsageBytes(size_t max_transfer_buffer_usage_bytes);
  size_t GetPeakTransferBufferMemoryUsedBytes() const {
    return peak_transfer_buffer_memory_used_bytes_;
  }

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

    void BindTexture(blink::WGC3Denum target, blink::WebGLId id);
    void UnbindTexture(blink::WebGLId id);

    blink::WebGLId BoundTexture(blink::WGC3Denum target);

   private:
    typedef base::hash_map<blink::WGC3Denum, blink::WebGLId> TargetTextureMap;
    TargetTextureMap bound_textures_;
  };

  struct Buffer {
    Buffer();
    ~Buffer();

    blink::WGC3Denum target;
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

  void CreateNamespace();
  blink::WebGLId BoundTextureId(blink::WGC3Denum target);
  scoped_refptr<TestTexture> BoundTexture(blink::WGC3Denum target);
  void CheckTextureIsBound(blink::WGC3Denum target);

  unsigned context_id_;
  Attributes attributes_;
  ContextProvider::Capabilities test_capabilities_;
  int times_bind_texture_succeeds_;
  int times_end_query_succeeds_;
  int times_gen_mailbox_succeeds_;
  bool context_lost_;
  int times_map_image_chromium_succeeds_;
  int times_map_buffer_chromium_succeeds_;
  WebGraphicsContextLostCallback* context_lost_callback_;
  base::hash_set<unsigned> used_textures_;
  unsigned next_program_id_;
  base::hash_set<unsigned> program_set_;
  unsigned next_shader_id_;
  base::hash_set<unsigned> shader_set_;
  std::vector<blink::WebGraphicsContext3D*> shared_contexts_;
  int max_texture_size_;
  bool reshape_called_;
  int width_;
  int height_;
  float scale_factor_;
  TestContextSupport* test_support_;
  gfx::Rect update_rect_;
  UpdateType last_update_type_;
  unsigned next_insert_sync_point_;
  unsigned last_waited_sync_point_;

  unsigned bound_buffer_;
  TextureTargets texture_targets_;

  size_t peak_transfer_buffer_memory_used_bytes_;

  scoped_refptr<Namespace> namespace_;
  static Namespace* shared_namespace_;

  base::WeakPtrFactory<TestWebGraphicsContext3D> weak_ptr_factory_;
};

}  // namespace cc

#endif  // CC_TEST_TEST_WEB_GRAPHICS_CONTEXT_3D_H_
