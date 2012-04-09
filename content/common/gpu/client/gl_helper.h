// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_CLIENT_GL_HELPER_H_
#define CONTENT_COMMON_GPU_CLIENT_GL_HELPER_H_
#pragma once

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebGraphicsContext3D.h"

namespace gfx {
class Size;
}

namespace content {

// Provides higher level operations on top of the WebKit::WebGraphicsContext3D
// interfaces.
class GLHelper {
 public:
  GLHelper(WebKit::WebGraphicsContext3D* context);
  virtual ~GLHelper();

  WebKit::WebGraphicsContext3D* context() const;

  // Detaches all the resources GLHelper manages.
  void Detach();

  // Copies the contents of |src_texture| with the size of |src_size| into
  // |out|. The contents is transformed so that it fits in |dst_size|.
  bool CopyTextureTo(WebKit::WebGLId src_texture,
                     const gfx::Size& src_size,
                     const gfx::Size& dst_size,
                     unsigned char* out);

  // Returns the shader compiled from the source.
  WebKit::WebGLId CompileShaderFromSource(const WebKit::WGC3Dchar* source,
                                          WebKit::WGC3Denum type);

 private:
  class CopyTextureToImpl;

  WebKit::WebGraphicsContext3D* context_;
  scoped_ptr<CopyTextureToImpl> copy_texture_to_impl_;

  DISALLOW_COPY_AND_ASSIGN(GLHelper);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_CLIENT_GL_HELPER_H_
