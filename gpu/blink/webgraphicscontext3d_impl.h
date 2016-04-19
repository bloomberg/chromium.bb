// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_BLINK_WEBGRAPHICSCONTEXT3D_IMPL_H_
#define GPU_BLINK_WEBGRAPHICSCONTEXT3D_IMPL_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "gpu/blink/gpu_blink_export.h"
#include "third_party/WebKit/public/platform/WebGraphicsContext3D.h"
#include "third_party/WebKit/public/platform/WebString.h"

namespace gpu {

namespace gles2 {
class GLES2Interface;
struct ContextCreationAttribHelper;
}
}

namespace gpu_blink {

class GPU_BLINK_EXPORT WebGraphicsContext3DImpl
    : public NON_EXPORTED_BASE(blink::WebGraphicsContext3D) {
 public:
  ~WebGraphicsContext3DImpl() override;

  //----------------------------------------------------------------------
  // WebGraphicsContext3D methods

  void setContextLostCallback(
      WebGraphicsContext3D::WebGraphicsContextLostCallback* callback);

  ::gpu::gles2::GLES2Interface* GetGLInterface() {
    return gl_;
  }

 protected:
  WebGraphicsContext3DImpl();

  void SetGLInterface(::gpu::gles2::GLES2Interface* gl) { gl_ = gl; }

  bool initialized_;
  bool initialize_failed_;

  WebGraphicsContextLostCallback* context_lost_callback_;

  ::gpu::gles2::GLES2Interface* gl_;
  bool lose_context_when_out_of_memory_;
};

}  // namespace gpu_blink

#endif  // GPU_BLINK_WEBGRAPHICSCONTEXT3D_IMPL_H_
