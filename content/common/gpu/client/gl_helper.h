// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_CLIENT_GL_HELPER_H_
#define CONTENT_COMMON_GPU_CLIENT_GL_HELPER_H_

#include "base/atomicops.h"
#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "content/common/gpu/client/webgraphicscontext3d_command_buffer_impl.h"

namespace gfx {
class Rect;
class Size;
}

class SkRegion;

namespace content {

// Provides higher level operations on top of the WebKit::WebGraphicsContext3D
// interfaces.
class GLHelper {
 public:
  explicit GLHelper(WebGraphicsContext3DCommandBufferImpl* context);
  virtual ~GLHelper();

  WebGraphicsContext3DCommandBufferImpl* context() const;

  // Copies the block of pixels specified with |src_subrect| from |src_texture|,
  // scales it to |dst_size|, and writes it into |out|.
  // |src_size| is the size of |src_texture|. The result is of format GL_BGRA
  // and is potentially flipped vertically to make it a correct image
  // representation.  |callback| is invoked with the copy result when the copy
  // operation has completed.
  void CropScaleReadbackAndCleanTexture(
      WebKit::WebGLId src_texture,
      const gfx::Size& src_size,
      const gfx::Rect& src_subrect,
      const gfx::Size& dst_size,
      unsigned char* out,
      const base::Callback<void(bool)>& callback);

  // Copies the texture data out of |texture| into |out|.  |size| is the
  // size of the texture.  No post processing is applied to the pixels.  The
  // texture is assumed to have a format of GL_RGBA with a pixel type of
  // GL_UNSIGNED_BYTE.  This is a blocking call that calls glReadPixels on this
  // current context.
  void ReadbackTextureSync(WebKit::WebGLId texture,
                           const gfx::Rect& src_rect,
                           unsigned char* out);

  // Creates a copy of the specified texture. |size| is the size of the texture.
  WebKit::WebGLId CopyTexture(WebKit::WebGLId texture,
                              const gfx::Size& size);

  // Creates a scaled copy of the specified texture. |src_size| is the size of
  // the texture and |dst_size| is the size of the resulting copy.
  WebKit::WebGLId CopyAndScaleTexture(WebKit::WebGLId texture,
                                      const gfx::Size& src_size,
                                      const gfx::Size& dst_size,
                                      bool vertically_flip_texture);

  // Returns the shader compiled from the source.
  WebKit::WebGLId CompileShaderFromSource(const WebKit::WGC3Dchar* source,
                                          WebKit::WGC3Denum type);

  // Copies all pixels from |previous_texture| into |texture| that are
  // inside the region covered by |old_damage| but not part of |new_damage|.
  void CopySubBufferDamage(WebKit::WebGLId texture,
                           WebKit::WebGLId previous_texture,
                           const SkRegion& new_damage,
                           const SkRegion& old_damage);

 private:
  class CopyTextureToImpl;

  // Creates |copy_texture_to_impl_| if NULL.
  void InitCopyTextToImpl();

  WebGraphicsContext3DCommandBufferImpl* context_;
  scoped_ptr<CopyTextureToImpl> copy_texture_to_impl_;

  DISALLOW_COPY_AND_ASSIGN(GLHelper);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_CLIENT_GL_HELPER_H_
