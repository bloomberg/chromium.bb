// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/content_renderer_pepper_host_factory.h"

#include "ppapi/host/resource_host.h"
#include "ppapi/proxy/ppapi_messages.h"

using ppapi::host::ResourceHost;

namespace content {

ContentRendererPepperHostFactory::ContentRendererPepperHostFactory(
    RenderViewImpl* render_view)
    : render_view_(render_view) {
}

ContentRendererPepperHostFactory::~ContentRendererPepperHostFactory() {
}

scoped_ptr<ResourceHost> ContentRendererPepperHostFactory::CreateResourceHost(
    ppapi::host::PpapiHost* host,
    const ppapi::proxy::ResourceMessageCallParams& params,
    PP_Instance instance,
    const IPC::Message& message) {
  // TODO(brettw) host creation goes here.
  return scoped_ptr<ResourceHost>();
}

}  // namespace content
