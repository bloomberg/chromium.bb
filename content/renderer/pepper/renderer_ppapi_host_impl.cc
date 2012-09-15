// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/renderer_ppapi_host_impl.h"

#include "base/logging.h"
#include "content/renderer/pepper/pepper_in_process_resource_creation.h"
#include "content/renderer/pepper/pepper_in_process_router.h"
#include "content/renderer/pepper/pepper_plugin_delegate_impl.h"
#include "content/renderer/render_view_impl.h"
#include "ppapi/proxy/host_dispatcher.h"
#include "webkit/plugins/ppapi/host_globals.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"

using webkit::ppapi::HostGlobals;
using webkit::ppapi::PluginInstance;

namespace content {

// Out-of-process constructor.
RendererPpapiHostImpl::RendererPpapiHostImpl(
    webkit::ppapi::PluginModule* module,
    ppapi::proxy::HostDispatcher* dispatcher,
    const ppapi::PpapiPermissions& permissions)
    : module_(module) {
  // Hook the PpapiHost up to the dispatcher for out-of-process communication.
  ppapi_host_.reset(
      new ppapi::host::PpapiHost(dispatcher, permissions));
  ppapi_host_->AddHostFactoryFilter(scoped_ptr<ppapi::host::HostFactory>(
      new ContentRendererPepperHostFactory(this)));
  dispatcher->AddFilter(ppapi_host_.get());
}

// In-process constructor.
RendererPpapiHostImpl::RendererPpapiHostImpl(
    webkit::ppapi::PluginModule* module,
    const ppapi::PpapiPermissions& permissions)
    : module_(module) {
  // Hook the host up to the in-process router.
  in_process_router_.reset(new PepperInProcessRouter(this));
  ppapi_host_.reset(new ppapi::host::PpapiHost(
      in_process_router_->GetRendererToPluginSender(), permissions));
  ppapi_host_->AddHostFactoryFilter(scoped_ptr<ppapi::host::HostFactory>(
      new ContentRendererPepperHostFactory(this)));
}

RendererPpapiHostImpl::~RendererPpapiHostImpl() {
}

// static
RendererPpapiHostImpl* RendererPpapiHostImpl::CreateOnModuleForOutOfProcess(
    webkit::ppapi::PluginModule* module,
    ppapi::proxy::HostDispatcher* dispatcher,
    const ppapi::PpapiPermissions& permissions) {
  DCHECK(!module->GetEmbedderState());
  RendererPpapiHostImpl* result = new RendererPpapiHostImpl(
      module, dispatcher, permissions);

  // Takes ownership of pointer.
  module->SetEmbedderState(
      scoped_ptr<webkit::ppapi::PluginModule::EmbedderState>(result));

  return result;
}

// static
RendererPpapiHostImpl* RendererPpapiHostImpl::CreateOnModuleForInProcess(
    webkit::ppapi::PluginModule* module,
    const ppapi::PpapiPermissions& permissions) {
  DCHECK(!module->GetEmbedderState());
  RendererPpapiHostImpl* result = new RendererPpapiHostImpl(
      module, permissions);

  // Takes ownership of pointer.
  module->SetEmbedderState(
      scoped_ptr<webkit::ppapi::PluginModule::EmbedderState>(result));

  return result;
}

scoped_ptr< ::ppapi::thunk::ResourceCreationAPI>
RendererPpapiHostImpl::CreateInProcessResourceCreationAPI(
    webkit::ppapi::PluginInstance* instance) {
  return scoped_ptr< ::ppapi::thunk::ResourceCreationAPI>(
      new PepperInProcessResourceCreation(this, instance));
}

ppapi::host::PpapiHost* RendererPpapiHostImpl::GetPpapiHost() {
  return ppapi_host_.get();
}

RenderView* RendererPpapiHostImpl::GetRenderViewForInstance(
    PP_Instance instance) const {
  PluginInstance* instance_object = GetAndValidateInstance(instance);
  if (!instance_object)
    return NULL;

  // Since we're the embedder, we can make assumptions about the delegate on
  // the instance and get back to our RenderView.
  return static_cast<PepperPluginDelegateImpl*>(
      instance_object->delegate())->render_view();
}

bool RendererPpapiHostImpl::IsValidInstance(
    PP_Instance instance) const {
  return !!GetAndValidateInstance(instance);
}

bool RendererPpapiHostImpl::HasUserGesture(PP_Instance instance) const {
  PluginInstance* instance_object = GetAndValidateInstance(instance);
  if (!instance_object)
    return false;

  if (instance_object->module()->permissions().HasPermission(
          ppapi::PERMISSION_BYPASS_USER_GESTURE))
    return true;
  return instance_object->IsProcessingUserGesture();
}

PluginInstance* RendererPpapiHostImpl::GetAndValidateInstance(
    PP_Instance pp_instance) const {
  PluginInstance* instance = HostGlobals::Get()->GetInstance(pp_instance);
  if (!instance)
    return NULL;
  if (instance->module() != module_)
    return NULL;
  return instance;
}

}  // namespace content
