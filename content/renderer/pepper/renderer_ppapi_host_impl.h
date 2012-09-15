// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_RENDERER_PPAPI_HOST_IMPL_H_
#define CONTENT_RENDERER_PEPPER_RENDERER_PPAPI_HOST_IMPL_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/renderer/renderer_ppapi_host.h"
#include "content/renderer/pepper/content_renderer_pepper_host_factory.h"
#include "ppapi/host/ppapi_host.h"
#include "webkit/plugins/ppapi/plugin_module.h"

namespace IPC {
class Sender;
}

namespace ppapi {

namespace proxy {
class HostDispatcher;
}

namespace thunk {
class ResourceCreationAPI;
}

}  // namespace ppapi

namespace webkit {
namespace ppapi {
class PluginInstance;
class PluginModule;
}
}

namespace content {

class PepperInProcessRouter;

// This class is attached to a PluginModule via the module's embedder state.
// The plugin module manages our lifetime.
class RendererPpapiHostImpl
    : public RendererPpapiHost,
      public webkit::ppapi::PluginModule::EmbedderState {
 public:
  virtual ~RendererPpapiHostImpl();

  // Factory functions to create in process or out-of-process host impls. The
  // host will be created and associated with the given module, which must not
  // already have embedder state on it.
  //
  // The module will take ownership of the new host impl. The returned value
  // does not pass ownership, it's just for the information of the caller.
  static RendererPpapiHostImpl* CreateOnModuleForOutOfProcess(
      webkit::ppapi::PluginModule* module,
      ppapi::proxy::HostDispatcher* dispatcher,
      const ppapi::PpapiPermissions& permissions);
  static RendererPpapiHostImpl* CreateOnModuleForInProcess(
      webkit::ppapi::PluginModule* module,
      const ppapi::PpapiPermissions& permissions);

  // Returns the router that we use for in-process IPC emulation (see the
  // pepper_in_process_router.h for more). This will be NULL when the plugin
  // is running out-of-process.
  PepperInProcessRouter* in_process_router() {
    return in_process_router_.get();
  }

  // Creates the in-process resource creation API wrapper for the given
  // plugin instance. This object will reference the host impl, so the
  // host impl should outlive the returned pointer. Since the resource
  // creation object is associated with the instance, this will generally
  // happen automatically.
  scoped_ptr< ::ppapi::thunk::ResourceCreationAPI>
      CreateInProcessResourceCreationAPI(
          webkit::ppapi::PluginInstance* instance);

  // RendererPpapiHost.
  virtual ppapi::host::PpapiHost* GetPpapiHost() OVERRIDE;
  virtual bool IsValidInstance(PP_Instance instance) const OVERRIDE;
  virtual RenderView* GetRenderViewForInstance(
      PP_Instance instance) const OVERRIDE;
  virtual bool HasUserGesture(PP_Instance instance) const OVERRIDE;

 private:
  RendererPpapiHostImpl(webkit::ppapi::PluginModule* module,
                        ppapi::proxy::HostDispatcher* dispatcher,
                        const ppapi::PpapiPermissions& permissions);
  RendererPpapiHostImpl(webkit::ppapi::PluginModule* module,
                        const ppapi::PpapiPermissions& permissions);

  // Retrieves the plugin instance object associated with the given PP_Instance
  // and validates that it is one of the instances associated with our module.
  // Returns NULL on failure.
  //
  // We use this to security check the PP_Instance values sent from a plugin to
  // make sure it's not trying to spoof another instance.
  webkit::ppapi::PluginInstance* GetAndValidateInstance(
      PP_Instance instance) const;

  webkit::ppapi::PluginModule* module_;  // Non-owning pointer.

  scoped_ptr<ppapi::host::PpapiHost> ppapi_host_;

  // Null when running out-of-process.
  scoped_ptr<PepperInProcessRouter> in_process_router_;

  DISALLOW_COPY_AND_ASSIGN(RendererPpapiHostImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_RENDERER_PPAPI_HOST_IMPL_H_
