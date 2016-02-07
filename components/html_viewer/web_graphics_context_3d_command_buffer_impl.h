// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HTML_VIEWER_WEB_GRAPHICS_CONTEXT_3D_COMMAND_BUFFER_IMPL_H_
#define COMPONENTS_HTML_VIEWER_WEB_GRAPHICS_CONTEXT_3D_COMMAND_BUFFER_IMPL_H_

#include "base/macros.h"
#include "components/mus/public/interfaces/command_buffer.mojom.h"
#include "gpu/blink/webgraphicscontext3d_impl.h"
#include "mojo/public/c/gles2/gles2.h"
#include "mojo/public/cpp/system/core.h"
#include "third_party/WebKit/public/platform/WebGraphicsContext3D.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "url/gurl.h"

namespace mojo {
class Shell;
}

namespace gpu {
  class ContextSupport;
  namespace gles2 { class GLES2Interface; }
}

namespace html_viewer {

class GlobalState;

class WebGraphicsContext3DCommandBufferImpl
    : public gpu_blink::WebGraphicsContext3DImpl {
 public:
  static WebGraphicsContext3DCommandBufferImpl* CreateOffscreenContext(
      GlobalState* global_state,
      mojo::Shell* shell,
      const GURL& active_url,
      const blink::WebGraphicsContext3D::Attributes& attributes,
      blink::WebGraphicsContext3D* share_context,
      blink::WebGraphicsContext3D::WebGraphicsInfo* gl_info);

 private:
  WebGraphicsContext3DCommandBufferImpl(
      mojo::InterfacePtrInfo<mus::mojom::CommandBuffer> command_buffer_info,
      const GURL& active_url,
      const blink::WebGraphicsContext3D::Attributes& attributes,
      blink::WebGraphicsContext3D* share_context);
  ~WebGraphicsContext3DCommandBufferImpl() override;

  static void ContextLostThunk(void* closure) {
    static_cast<WebGraphicsContext3DCommandBufferImpl*>(closure)->ContextLost();
  }
  void ContextLost();

  MojoGLES2Context gles2_context_;
  scoped_ptr<gpu::gles2::GLES2Interface> context_gl_;

  DISALLOW_COPY_AND_ASSIGN(WebGraphicsContext3DCommandBufferImpl);
};

}  // namespace html_viewer

#endif  // COMPONENTS_HTML_VIEWER_WEB_GRAPHICS_CONTEXT_3D_COMMAND_BUFFER_IMPL_H_
