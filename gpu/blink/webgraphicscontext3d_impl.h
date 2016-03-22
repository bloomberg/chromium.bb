// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_BLINK_WEBGRAPHICSCONTEXT3D_IMPL_H_
#define GPU_BLINK_WEBGRAPHICSCONTEXT3D_IMPL_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "gpu/blink/gpu_blink_export.h"
#include "third_party/WebKit/public/platform/WebGraphicsContext3D.h"
#include "third_party/WebKit/public/platform/WebString.h"

namespace gpu {

namespace gles2 {
class GLES2Interface;
class GLES2ImplementationErrorMessageCallback;
struct ContextCreationAttribHelper;
}
}

namespace gpu_blink {

class WebGraphicsContext3DErrorMessageCallback;

class GPU_BLINK_EXPORT WebGraphicsContext3DImpl
    : public NON_EXPORTED_BASE(blink::WebGraphicsContext3D) {
 public:
  ~WebGraphicsContext3DImpl() override;

  //----------------------------------------------------------------------
  // WebGraphicsContext3D methods

  bool getActiveAttrib(blink::WebGLId program,
                       blink::WGC3Duint index,
                       ActiveInfo&) override;
  bool getActiveUniform(blink::WebGLId program,
                        blink::WGC3Duint index,
                        ActiveInfo&) override;

  blink::WebString getProgramInfoLog(blink::WebGLId program) override;
  blink::WebString getShaderInfoLog(blink::WebGLId shader) override;
  blink::WebString getShaderSource(blink::WebGLId shader) override;
  blink::WebString getString(blink::WGC3Denum name) override;

  void shaderSource(blink::WebGLId shader,
                    const blink::WGC3Dchar* string) override;

  blink::WebString getRequestableExtensionsCHROMIUM() override;

  blink::WebString getTranslatedShaderSourceANGLE(
      blink::WebGLId shader) override;

  void setContextLostCallback(
      WebGraphicsContext3D::WebGraphicsContextLostCallback* callback) override;

  void setErrorMessageCallback(
      WebGraphicsContext3D::WebGraphicsErrorMessageCallback* callback) override;

  void pushGroupMarkerEXT(const blink::WGC3Dchar* marker) override;

  // WebGraphicsContext3D implementation.
  ::gpu::gles2::GLES2Interface* getGLES2Interface() override;

  ::gpu::gles2::GLES2Interface* GetGLInterface() {
    return gl_;
  }

  // Convert WebGL context creation attributes into command buffer / EGL size
  // requests.
  static void ConvertAttributes(
      const blink::WebGraphicsContext3D::Attributes& attributes,
      ::gpu::gles2::ContextCreationAttribHelper* output_attribs);

 protected:
  friend class WebGraphicsContext3DErrorMessageCallback;

  WebGraphicsContext3DImpl();

  ::gpu::gles2::GLES2ImplementationErrorMessageCallback*
      getErrorMessageCallback();
  virtual void OnErrorMessage(const std::string& message, int id);

  void SetGLInterface(::gpu::gles2::GLES2Interface* gl) { gl_ = gl; }

  bool initialized_;
  bool initialize_failed_;

  WebGraphicsContext3D::WebGraphicsContextLostCallback* context_lost_callback_;

  WebGraphicsContext3D::WebGraphicsErrorMessageCallback*
      error_message_callback_;
  scoped_ptr<WebGraphicsContext3DErrorMessageCallback>
      client_error_message_callback_;

  // Errors raised by synthesizeGLError().
  std::vector<blink::WGC3Denum> synthetic_errors_;

  ::gpu::gles2::GLES2Interface* gl_;
  bool lose_context_when_out_of_memory_;
};

}  // namespace gpu_blink

#endif  // GPU_BLINK_WEBGRAPHICSCONTEXT3D_IMPL_H_
