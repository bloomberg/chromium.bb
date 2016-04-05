// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/blink/webgraphicscontext3d_impl.h"

#include <stdint.h>

#include "base/atomicops.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/sys_info.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/client/gles2_lib.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/skia_bindings/gl_bindings_skia_cmd_buffer.h"

#include "third_party/khronos/GLES2/gl2.h"
#ifndef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES 1
#endif
#include "third_party/khronos/GLES2/gl2ext.h"

namespace gpu_blink {

class WebGraphicsContext3DErrorMessageCallback
    : public ::gpu::gles2::GLES2ImplementationErrorMessageCallback {
 public:
  WebGraphicsContext3DErrorMessageCallback(
      WebGraphicsContext3DImpl* context)
      : graphics_context_(context) {
  }

  void OnErrorMessage(const char* msg, int id) override;

 private:
  WebGraphicsContext3DImpl* graphics_context_;

  DISALLOW_COPY_AND_ASSIGN(WebGraphicsContext3DErrorMessageCallback);
};

void WebGraphicsContext3DErrorMessageCallback::OnErrorMessage(
    const char* msg, int id) {
  graphics_context_->OnErrorMessage(msg, id);
}

WebGraphicsContext3DImpl::WebGraphicsContext3DImpl()
    : initialized_(false),
      initialize_failed_(false),
      context_lost_callback_(0),
      error_message_callback_(0),
      gl_(NULL) {}

WebGraphicsContext3DImpl::~WebGraphicsContext3DImpl() {

}

void WebGraphicsContext3DImpl::setErrorMessageCallback(
    WebGraphicsContext3D::WebGraphicsErrorMessageCallback* cb) {
  error_message_callback_ = cb;
}

void WebGraphicsContext3DImpl::setContextLostCallback(
    WebGraphicsContext3D::WebGraphicsContextLostCallback* cb) {
  context_lost_callback_ = cb;
}

::gpu::gles2::GLES2ImplementationErrorMessageCallback*
    WebGraphicsContext3DImpl::getErrorMessageCallback() {
  if (!client_error_message_callback_) {
    client_error_message_callback_.reset(
        new WebGraphicsContext3DErrorMessageCallback(this));
  }
  return client_error_message_callback_.get();
}

void WebGraphicsContext3DImpl::OnErrorMessage(
    const std::string& message, int id) {
  if (error_message_callback_) {
    blink::WebString str = blink::WebString::fromUTF8(message.c_str());
    error_message_callback_->onErrorMessage(str, id);
  }
}

}  // namespace gpu_blink
