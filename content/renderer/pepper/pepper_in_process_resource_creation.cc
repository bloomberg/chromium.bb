// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/pepper_in_process_resource_creation.h"

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "content/renderer/render_view_impl.h"
#include "content/renderer/pepper/pepper_in_process_router.h"
#include "content/renderer/pepper/renderer_ppapi_host_impl.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/proxy/file_chooser_resource.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/printing_resource.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/ppapi_permissions.h"
#include "ppapi/shared_impl/resource_tracker.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"

// Note that the code in the creation functions in this file should generally
// be the same as that in ppapi/proxy/resource_creation_proxy.cc. See
// pepper_in_process_resource_creation.h for what this file is for.

namespace content {

// PepperInProcessResourceCreation --------------------------------------------

PepperInProcessResourceCreation::PepperInProcessResourceCreation(
    RendererPpapiHostImpl* host_impl,
    webkit::ppapi::PluginInstance* instance)
    : ResourceCreationImpl(instance),
      host_impl_(host_impl) {
}

PepperInProcessResourceCreation::~PepperInProcessResourceCreation() {
}

PP_Resource PepperInProcessResourceCreation::CreateFileChooser(
    PP_Instance instance,
    PP_FileChooserMode_Dev mode,
    const char* accept_types) {
  return (new ppapi::proxy::FileChooserResource(
      host_impl_->in_process_router()->GetPluginConnection(),
      instance, mode, accept_types))->GetReference();
}

PP_Resource PepperInProcessResourceCreation::CreatePrinting(
    PP_Instance instance) {
  return (new ppapi::proxy::PrintingResource(
      host_impl_->in_process_router()->GetPluginConnection(),
      instance))->GetReference();
}

}  // namespace content
