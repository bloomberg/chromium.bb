// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_CLIENT_GL_HELPER_H_
#define CONTENT_COMMON_GPU_CLIENT_GL_HELPER_H_

#include "base/atomicops.h"
#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebGraphicsContext3D.h"

namespace gfx {
class Rect;
class Size;
}

namespace content {

// Provides higher level operations on top of the WebKit::WebGraphicsContext3D
// interfaces.
class GLHelper {
 public:
  GLHelper(WebKit::WebGraphicsContext3D* context,
           WebKit::WebGraphicsContext3D* context_for_thread);
  virtual ~GLHelper();

  WebKit::WebGraphicsContext3D* context() const;

  // Copies the block of pixels specified with |src_subrect| from |src_texture|,
  // scales it to |dst_size|, and writes it into |out|.
  // |src_size| is the size of |src_texture|. |callback| is invoked with the
  // copy result when the copy operation has completed.
  void CopyTextureTo(WebKit::WebGLId src_texture,
                     const gfx::Size& src_size,
                     const gfx::Rect& src_subrect,
                     const gfx::Size& dst_size,
                     unsigned char* out,
                     const base::Callback<void(bool)>& callback);

  // Creates a copy of the specified texture. |size| is the size of the texture.
  WebKit::WebGLId CopyTexture(WebKit::WebGLId texture,
                              const gfx::Size& size);

  // Returns the shader compiled from the source.
  WebKit::WebGLId CompileShaderFromSource(const WebKit::WGC3Dchar* source,
                                          WebKit::WGC3Denum type);

 private:
  class CopyTextureToImpl;

  // Creates |copy_texture_to_impl_| if NULL.
  void InitCopyTextToImpl();

  WebKit::WebGraphicsContext3D* context_;
  WebKit::WebGraphicsContext3D* context_for_thread_;
  scoped_ptr<CopyTextureToImpl> copy_texture_to_impl_;

  // The number of all GLHelper instances.
  static base::subtle::Atomic32 count_;

  DISALLOW_COPY_AND_ASSIGN(GLHelper);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_CLIENT_GL_HELPER_H_
