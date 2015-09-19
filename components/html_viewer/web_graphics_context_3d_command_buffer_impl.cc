// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/html_viewer/web_graphics_context_3d_command_buffer_impl.h"

#include "components/mus/public/interfaces/gpu.mojom.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "mojo/application/public/cpp/application_impl.h"
#include "mojo/cc/context_provider_mojo.h"
#include "mojo/gles2/gles2_context.h"
#include "mojo/gpu/mojo_gles2_impl_autogen.h"
#include "third_party/mojo/src/mojo/public/cpp/environment/environment.h"

namespace html_viewer {

WebGraphicsContext3DCommandBufferImpl::WebGraphicsContext3DCommandBufferImpl(
    mojo::ApplicationImpl* app,
    const GURL& active_url,
    const blink::WebGraphicsContext3D::Attributes& attributes,
    blink::WebGraphicsContext3D* share_context,
    blink::WebGLInfo* gl_info) {
  mojo::URLRequestPtr request(mojo::URLRequest::New());
  request->url = mojo::String::From("mojo:mus");
  mojo::GpuPtr gpu_service;
  app->ConnectToService(request.Pass(), &gpu_service);

  mojo::CommandBufferPtr cb;
  gpu_service->CreateOffscreenGLES2Context(GetProxy(&cb));
  command_buffer_handle_ = cb.PassInterface().PassHandle();
  CHECK(command_buffer_handle_.is_valid());
  // TODO(penghuang): Support share context.
  // TODO(penghuang): Fill gl_info.
  gpu::gles2::ContextCreationAttribHelper attrib_helper;
  attrib_helper.alpha_size = attributes.alpha ? 8 : 0;
  attrib_helper.depth_size = attributes.depth ? 24 : 0;
  attrib_helper.stencil_size = attributes.stencil ? 8 : 0;
  attrib_helper.samples = attributes.antialias ? 4 : 0;
  attrib_helper.sample_buffers = attributes.antialias ? 1 : 0;
  attrib_helper.fail_if_major_perf_caveat =
      attributes.failIfMajorPerformanceCaveat;
  attrib_helper.bind_generates_resource = false;
  attrib_helper.webgl_version = attributes.webGLVersion;
  std::vector<int32_t> attrib_vector;
  attrib_helper.Serialize(&attrib_vector);
  gles2_context_ = MojoGLES2CreateContext(
      command_buffer_handle_.release().value(),
      attrib_vector.data(),
      &ContextLostThunk,
      this,
      mojo::Environment::GetDefaultAsyncWaiter());
  context_gl_.reset(new mojo::MojoGLES2Impl(gles2_context_));
  setGLInterface(context_gl_.get());
}

WebGraphicsContext3DCommandBufferImpl::
    ~WebGraphicsContext3DCommandBufferImpl() {
  if (gles2_context_)
    MojoGLES2DestroyContext(gles2_context_);
}

// static
WebGraphicsContext3DCommandBufferImpl*
WebGraphicsContext3DCommandBufferImpl::CreateOffscreenContext(
    mojo::ApplicationImpl* app,
    const GURL& active_url,
    const blink::WebGraphicsContext3D::Attributes& attributes,
    blink::WebGraphicsContext3D* share_context,
    blink::WebGLInfo* gl_info) {
  return new WebGraphicsContext3DCommandBufferImpl(
      app, active_url, attributes, share_context, gl_info);
}

void WebGraphicsContext3DCommandBufferImpl::ContextLost() {
  if (context_lost_callback_)
    context_lost_callback_->onContextLost();
}

}  // namespace html_viewer
