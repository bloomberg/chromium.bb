// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_CLIENT_GL_HELPER_H_
#define CONTENT_COMMON_GPU_CLIENT_GL_HELPER_H_
#pragma once

#include "base/atomicops.h"
#include "base/basictypes.h"
#include "base/callback.h"
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
  GLHelper(WebKit::WebGraphicsContext3D* context,
           WebKit::WebGraphicsContext3D* context_for_thread);
  virtual ~GLHelper();

  WebKit::WebGraphicsContext3D* context() const;

  // Copies the contents of |src_texture| with the size of |src_size| into
  // |out|. The contents is transformed so that it fits in |dst_size|.
  // |callback| is invoked with the copy result when the copy operation has
  //  completed.
  void CopyTextureTo(WebKit::WebGLId src_texture,
                     const gfx::Size& src_size,
                     const gfx::Size& dst_size,
                     unsigned char* out,
                     const base::Callback<void(bool)>& callback);

  // Returns the shader compiled from the source.
  WebKit::WebGLId CompileShaderFromSource(const WebKit::WGC3Dchar* source,
                                          WebKit::WGC3Denum type);

 private:
  class CopyTextureToImpl;

  WebKit::WebGraphicsContext3D* context_;
  WebKit::WebGraphicsContext3D* context_for_thread_;
  scoped_ptr<CopyTextureToImpl> copy_texture_to_impl_;

  // The number of all GLHelper instances.
  static base::subtle::Atomic32 count_;

  DISALLOW_COPY_AND_ASSIGN(GLHelper);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_CLIENT_GL_HELPER_H_
