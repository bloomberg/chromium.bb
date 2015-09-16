// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HTML_VIEWER_WEB_GRAPHICS_CONTEXT_3D_COMMAND_BUFFER_IMPL_H_
#define COMPONENTS_HTML_VIEWER_WEB_GRAPHICS_CONTEXT_3D_COMMAND_BUFFER_IMPL_H_

#include "base/macros.h"
#include "gpu/blink/webgraphicscontext3d_impl.h"
#include "mojo/public/c/gles2/gles2.h"
#include "third_party/WebKit/public/platform/WebGraphicsContext3D.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/mojo/src/mojo/public/cpp/system/core.h"
#include "third_party/mojo/src/mojo/public/cpp/system/message_pipe.h"
#include "url/gurl.h"

namespace gpu {
  class ContextSupport;
  namespace gles2 { class GLES2Interface; }
}

namespace mojo {
class ApplicationImpl;
}  // namespace mojo

namespace html_viewer {

class WebGraphicsContext3DCommandBufferImpl
    : public gpu_blink::WebGraphicsContext3DImpl {
 public:
  WebGraphicsContext3DCommandBufferImpl(
      mojo::ApplicationImpl* app,
      const GURL& active_url,
      const blink::WebGraphicsContext3D::Attributes& attributes,
      blink::WebGraphicsContext3D* share_context,
      blink::WebGLInfo* gl_info);
  virtual ~WebGraphicsContext3DCommandBufferImpl();

  static WebGraphicsContext3DCommandBufferImpl* CreateOffscreenContext(
      mojo::ApplicationImpl* app,
      const GURL& active_url,
      const blink::WebGraphicsContext3D::Attributes& attributes,
      blink::WebGraphicsContext3D* share_context,
      blink::WebGLInfo* gl_info);

 private:
  static void ContextLostThunk(void* closure) {
    static_cast<WebGraphicsContext3DCommandBufferImpl*>(closure)->ContextLost();
  }
  void ContextLost();

  mojo::ScopedMessagePipeHandle command_buffer_handle_;
  MojoGLES2Context gles2_context_;
  scoped_ptr<gpu::gles2::GLES2Interface> context_gl_;

  DISALLOW_COPY_AND_ASSIGN(WebGraphicsContext3DCommandBufferImpl);
};

}  // namespace html_viewer

#endif  // COMPONENTS_HTML_VIEWER_WEB_GRAPHICS_CONTEXT_3D_COMMAND_BUFFER_IMPL_H_
