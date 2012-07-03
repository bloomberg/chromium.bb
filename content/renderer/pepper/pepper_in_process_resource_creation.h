// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PEPPER_IN_PROCESS_RESOURCE_CREATION_H_
#define CONTENT_RENDERER_PEPPER_PEPPER_IN_PROCESS_RESOURCE_CREATION_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "webkit/plugins/ppapi/resource_creation_impl.h"

class RenderViewImpl;

namespace content {

// This class provides creation functions for the new resources with IPC
// backends that live in content/renderer/pepper.
//
// This is a bit confusing. The "old-style" resources live in
// webkit/plugins/ppapi and are created by the ResourceCreationImpl in that
// directory. The "new-style" IPC-only resources are in ppapi/proxy and are
// created by the RessourceCreationProxy in that directory.
//
// This class allows us to run new-style IPC-only resources in-process. We have
// an IPC reflector to run it in process. But then we have a problem with
// allocating the resources since src/webkit can't depend on IPC or see our IPC
// backend in content. This class overrides the normal in-process resource
// creation and adds in the resources that we implement in ppapi/proxy.
//
// When we convert all resources to use the new-style, we can just use the
// ResourceCreationProxy for all resources. This class is just glue to manage
// the temporary "two different cases."
class PepperInProcessResourceCreation
    : public webkit::ppapi::ResourceCreationImpl {
 public:
  PepperInProcessResourceCreation(RenderViewImpl* render_view,
                                  webkit::ppapi::PluginInstance* instance);
  virtual ~PepperInProcessResourceCreation();

 private:
  class HostToPluginRouter;
  scoped_ptr<HostToPluginRouter> host_to_plugin_router_;

  class PluginToHostRouter;
  scoped_ptr<PluginToHostRouter> plugin_to_host_router_;

  DISALLOW_COPY_AND_ASSIGN(PepperInProcessResourceCreation);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_PEPPER_IN_PROCESS_RESOURCE_CREATION_H_
