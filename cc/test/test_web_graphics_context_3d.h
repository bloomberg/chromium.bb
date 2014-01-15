// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_TEST_WEB_GRAPHICS_CONTEXT_3D_H_
#define CC_TEST_TEST_WEB_GRAPHICS_CONTEXT_3D_H_

#include <vector>

#include "base/callback.h"
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

  void set_context_lost_callback(const base::Closure& callback) {
    context_lost_callback_ = callback;
  }

  virtual void reshapeWithScaleFactor(int width,
                                      int height,
                                      float scale_factor);

  virtual bool isContextLost() OVERRIDE;

  virtual void attachShader(GLuint program, GLuint shader) OVERRIDE;
  virtual void bindFramebuffer(
      GLenum target, GLuint framebuffer) OVERRIDE;
  virtual void bindRenderbuffer(
      GLenum target, GLuint renderbuffer) OVERRIDE;
  virtual void bindTexture(
      GLenum target,
      GLuint texture_id) OVERRIDE;

  virtual void texParameteri(GLenum target,
                             GLenum pname,
                             GLint param) OVERRIDE;
  virtual void getTexParameteriv(GLenum target,
                                 GLenum pname,
                                 GLint* value) OVERRIDE;
  virtual void asyncTexImage2DCHROMIUM(GLenum target,
                                       GLint level,
                                       GLenum internalformat,
                                       GLsizei width,
                                       GLsizei height,
                                       GLint border,
                                       GLenum format,
                                       GLenum type,
                                       const void* pixels) {}
  virtual void asyncTexSubImage2DCHROMIUM(GLenum target,
                                          GLint level,
                                          GLint xoffset,
                                          GLint yoffset,
                                          GLsizei width,
                                          GLsizei height,
                                          GLenum format,
                                          GLenum type,
                                          const void* pixels) {}
  virtual void waitAsyncTexImage2DCHROMIUM(GLenum target) {}
  virtual void releaseTexImage2DCHROMIUM(GLenum target, GLint image_id) {}

  virtual GLenum checkFramebufferStatus(GLenum target) OVERRIDE;

  virtual GLint getUniformLocation(
      GLuint program,
      const GLchar* name) OVERRIDE;
  virtual GLsizeiptr getVertexAttribOffset(
      GLuint index,
      GLenum pname) OVERRIDE;

  virtual GLboolean isBuffer(GLuint buffer) OVERRIDE;
  virtual GLboolean isEnabled(GLenum cap) OVERRIDE;
  virtual GLboolean isFramebuffer(GLuint framebuffer) OVERRIDE;
  virtual GLboolean isProgram(GLuint program) OVERRIDE;
  virtual GLboolean isRenderbuffer(GLuint renderbuffer) OVERRIDE;
  virtual GLboolean isShader(GLuint shader) OVERRIDE;
  virtual GLboolean isTexture(GLuint texture) OVERRIDE;

  virtual void useProgram(GLuint program) OVERRIDE;

  virtual void genBuffers(GLsizei count, GLuint* ids) OVERRIDE;
  virtual void genFramebuffers(GLsizei count, GLuint* ids) OVERRIDE;
  virtual void genRenderbuffers(GLsizei count, GLuint* ids) OVERRIDE;
  virtual void genTextures(GLsizei count, GLuint* ids) OVERRIDE;

  virtual void deleteBuffers(GLsizei count, GLuint* ids) OVERRIDE;
  virtual void deleteFramebuffers(
      GLsizei count, GLuint* ids) OVERRIDE;
  virtual void deleteRenderbuffers(
      GLsizei count, GLuint* ids) OVERRIDE;
  virtual void deleteTextures(GLsizei count, GLuint* ids) OVERRIDE;

  virtual GLuint createBuffer() OVERRIDE;
  virtual GLuint createFramebuffer() OVERRIDE;
  virtual GLuint createRenderbuffer() OVERRIDE;
  virtual GLuint createTexture() OVERRIDE;

  virtual void deleteBuffer(GLuint id) OVERRIDE;
  virtual void deleteFramebuffer(GLuint id) OVERRIDE;
  virtual void deleteRenderbuffer(GLuint id) OVERRIDE;
  virtual void deleteTexture(GLuint id) OVERRIDE;

  virtual GLuint createProgram() OVERRIDE;
  virtual GLuint createShader(GLenum) OVERRIDE;
  virtual GLuint createExternalTexture();

  virtual void deleteProgram(GLuint id) OVERRIDE;
  virtual void deleteShader(GLuint id) OVERRIDE;

  virtual void endQueryEXT(GLenum target) OVERRIDE;
  virtual void getQueryObjectuivEXT(
      GLuint query,
      GLenum pname,
      GLuint* params) OVERRIDE;

  virtual void getIntegerv(
      GLenum pname,
      GLint* value) OVERRIDE;

  virtual void genMailboxCHROMIUM(GLbyte* mailbox);
  virtual void produceTextureCHROMIUM(GLenum target,
                                      const GLbyte* mailbox) { }
  virtual void consumeTextureCHROMIUM(GLenum target,
                                      const GLbyte* mailbox) { }

  virtual void loseContextCHROMIUM(GLenum current,
                                   GLenum other) OVERRIDE;

  virtual void finish() OVERRIDE;
  virtual void flush() OVERRIDE;

  virtual void bindBuffer(GLenum target, GLuint buffer) OVERRIDE;
  virtual void bufferData(GLenum target,
                          GLsizeiptr size,
                          const void* data,
                          GLenum usage) OVERRIDE;
  virtual void* mapBufferCHROMIUM(GLenum target,
                                  GLenum access);
  virtual GLboolean unmapBufferCHROMIUM(GLenum target);

  virtual GLuint createImageCHROMIUM(
      GLsizei width,
      GLsizei height,
      GLenum internalformat) OVERRIDE;
  virtual void destroyImageCHROMIUM(GLuint image_id) OVERRIDE;
  virtual void getImageParameterivCHROMIUM(
      GLuint image_id,
      GLenum pname,
      GLint* params) OVERRIDE;
  virtual void* mapImageCHROMIUM(
      GLuint image_id,
      GLenum access) OVERRIDE;
  virtual void unmapImageCHROMIUM(GLuint image_id) OVERRIDE;
  virtual void texImageIOSurface2DCHROMIUM(GLenum target,
                                           GLsizei width,
                                           GLsizei height,
                                           GLuint io_surface_id,
                                           GLuint plane) {}

  virtual unsigned insertSyncPoint();
  virtual void waitSyncPoint(unsigned sync_point);

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
  GLuint TextureAt(int i) const;

  size_t NumUsedTextures() const { return used_textures_.size(); }
  bool UsedTexture(int texture) const {
    return ContainsKey(used_textures_, texture);
  }
  void ResetUsedTextures() { used_textures_.clear(); }

  void set_have_extension_io_surface(bool have) {
    test_capabilities_.gpu.iosurface = have;
    test_capabilities_.gpu.texture_rectangle = have;
  }
  void set_have_extension_egl_image(bool have) {
    test_capabilities_.gpu.egl_image_external = have;
  }
  void set_have_post_sub_buffer(bool have) {
    test_capabilities_.gpu.post_sub_buffer = have;
  }
  void set_have_discard_framebuffer(bool have) {
    test_capabilities_.gpu.discard_framebuffer = have;
  }
  void set_support_compressed_texture_etc1(bool support) {
    test_capabilities_.gpu.texture_format_etc1 = support;
  }
  void set_support_texture_storage(bool support) {
    test_capabilities_.gpu.texture_storage = support;
  }

  // When this context is lost, all contexts in its share group are also lost.
  void add_share_group_context(TestWebGraphicsContext3D* context3d) {
    shared_contexts_.push_back(context3d);
  }

  void set_max_texture_size(int size) { max_texture_size_ = size; }

  static const GLuint kExternalTextureId;
  virtual GLuint NextTextureId();
  virtual void RetireTextureId(GLuint id);

  virtual GLuint NextBufferId();
  virtual void RetireBufferId(GLuint id);

  virtual GLuint NextImageId();
  virtual void RetireImageId(GLuint id);

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

  UpdateType last_update_type() { return last_update_type_; }

 protected:
  struct TextureTargets {
    TextureTargets();
    ~TextureTargets();

    void BindTexture(GLenum target, GLuint id);
    void UnbindTexture(GLuint id);

    GLuint BoundTexture(GLenum target);

   private:
    typedef base::hash_map<GLenum, GLuint> TargetTextureMap;
    TargetTextureMap bound_textures_;
  };

  struct Buffer {
    Buffer();
    ~Buffer();

    GLenum target;
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
  GLuint BoundTextureId(GLenum target);
  scoped_refptr<TestTexture> BoundTexture(GLenum target);
  void CheckTextureIsBound(GLenum target);

  unsigned context_id_;
  ContextProvider::Capabilities test_capabilities_;
  int times_bind_texture_succeeds_;
  int times_end_query_succeeds_;
  int times_gen_mailbox_succeeds_;
  bool context_lost_;
  int times_map_image_chromium_succeeds_;
  int times_map_buffer_chromium_succeeds_;
  base::Closure context_lost_callback_;
  base::hash_set<unsigned> used_textures_;
  unsigned next_program_id_;
  base::hash_set<unsigned> program_set_;
  unsigned next_shader_id_;
  base::hash_set<unsigned> shader_set_;
  std::vector<TestWebGraphicsContext3D*> shared_contexts_;
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
