// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/html_viewer/web_graphics_context_3d_command_buffer_impl.h"

#include "components/html_viewer/blink_basic_type_converters.h"
#include "components/html_viewer/global_state.h"
#include "components/mus/public/cpp/context_provider.h"
#include "components/mus/public/interfaces/gpu.mojom.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "mojo/application/public/cpp/application_impl.h"
#include "mojo/gles2/gles2_context.h"
#include "mojo/gpu/mojo_gles2_impl_autogen.h"
#include "third_party/mojo/src/mojo/public/cpp/environment/environment.h"
#include "third_party/mojo/src/mojo/public/cpp/system/message_pipe.h"

namespace html_viewer {

// static
WebGraphicsContext3DCommandBufferImpl*
WebGraphicsContext3DCommandBufferImpl::CreateOffscreenContext(
    GlobalState* global_state,
    mojo::ApplicationImpl* app,
    const GURL& active_url,
    const blink::WebGraphicsContext3D::Attributes& attributes,
    blink::WebGraphicsContext3D* share_context,
    blink::WebGraphicsContext3D::WebGraphicsInfo* gl_info) {
  DCHECK(gl_info);
  const mus::mojom::GpuInfo* gpu_info = global_state->GetGpuInfo();
  gl_info->vendorId = gpu_info->vendor_id;
  gl_info->deviceId = gpu_info->device_id;
  gl_info->vendorInfo = gpu_info->vendor_info.To<blink::WebString>();
  gl_info->rendererInfo = gpu_info->renderer_info.To<blink::WebString>();
  gl_info->driverVersion = gpu_info->driver_version.To<blink::WebString>();

  mojo::URLRequestPtr request(mojo::URLRequest::New());
  request->url = mojo::String::From("mojo:mus");
  mus::mojom::GpuPtr gpu_service;
  app->ConnectToService(request.Pass(), &gpu_service);
  mus::mojom::CommandBufferPtr cb;
  gpu_service->CreateOffscreenGLES2Context(GetProxy(&cb));
  return new WebGraphicsContext3DCommandBufferImpl(
      cb.PassInterface(), active_url, attributes, share_context);
}

WebGraphicsContext3DCommandBufferImpl::WebGraphicsContext3DCommandBufferImpl(
    mojo::InterfacePtrInfo<mus::mojom::CommandBuffer> command_buffer_info,
    const GURL& active_url,
    const blink::WebGraphicsContext3D::Attributes& attributes,
    blink::WebGraphicsContext3D* share_context) {
  mojo::ScopedMessagePipeHandle handle(command_buffer_info.PassHandle());
  CHECK(handle.is_valid());
  // TODO(penghuang): Support share context.
  gpu::gles2::ContextCreationAttribHelper attrib_helper;
  ConvertAttributes(attributes, &attrib_helper);
  std::vector<int32_t> attrib_vector;
  attrib_helper.Serialize(&attrib_vector);
  gles2_context_ = MojoGLES2CreateContext(
      handle.release().value(),
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

void WebGraphicsContext3DCommandBufferImpl::ContextLost() {
  if (context_lost_callback_)
    context_lost_callback_->onContextLost();
}

}  // namespace html_viewer
