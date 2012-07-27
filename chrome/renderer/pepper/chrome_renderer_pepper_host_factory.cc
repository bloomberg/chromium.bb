// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/pepper/chrome_renderer_pepper_host_factory.h"

#include "ppapi/host/resource_host.h"
#include "ppapi/proxy/ppapi_messages.h"

using ppapi::host::ResourceHost;

namespace chrome {

ChromeRendererPepperHostFactory::ChromeRendererPepperHostFactory() {
}

ChromeRendererPepperHostFactory::~ChromeRendererPepperHostFactory() {
}

scoped_ptr<ResourceHost>
ChromeRendererPepperHostFactory::CreateResourceHost(
    ppapi::host::PpapiHost* host,
    const ppapi::proxy::ResourceMessageCallParams& params,
    PP_Instance instance,
    const IPC::Message& message) {
  // There are no Chrome-side implementations of resources.
  return scoped_ptr<ResourceHost>();
}

}  // namespace chrome
