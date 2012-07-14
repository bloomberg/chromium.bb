// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/content_renderer_pepper_host_factory.h"

#include "content/renderer/pepper/pepper_instance_state_accessor.h"
#include "content/renderer/pepper/pepper_file_chooser_host.h"
#include "ppapi/host/resource_host.h"
#include "ppapi/proxy/ppapi_messages.h"

using ppapi::host::ResourceHost;

namespace content {

ContentRendererPepperHostFactory::ContentRendererPepperHostFactory(
    RenderViewImpl* render_view,
    const ppapi::PpapiPermissions& permissions,
    PepperInstanceStateAccessor* state)
    : render_view_(render_view),
      permissions_(permissions),
      instance_state_(state) {
}

ContentRendererPepperHostFactory::~ContentRendererPepperHostFactory() {
}

scoped_ptr<ResourceHost> ContentRendererPepperHostFactory::CreateResourceHost(
    ppapi::host::PpapiHost* host,
    const ppapi::proxy::ResourceMessageCallParams& params,
    PP_Instance instance,
    const IPC::Message& message) {
  // Make sure the plugin is giving us a valid instance for this resource.
  if (!instance_state_->IsValidInstance(instance))
    return scoped_ptr<ResourceHost>();

  // Resources for dev interfaces.
  // TODO(brettw) when we support any public or private interfaces, put them in
  // a separate switch above.
  if (permissions_.HasPermission(ppapi::PERMISSION_DEV)) {
    switch (message.type()) {
      case PpapiHostMsg_FileChooser_Create::ID:
        return scoped_ptr<ResourceHost>(new PepperFileChooserHost(
            host, instance, params.pp_resource(), render_view_,
            instance_state_));
    }
  }
  return scoped_ptr<ResourceHost>();
}

}  // namespace content
