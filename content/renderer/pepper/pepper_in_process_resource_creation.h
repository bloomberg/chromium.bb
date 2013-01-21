// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PEPPER_IN_PROCESS_RESOURCE_CREATION_H_
#define CONTENT_RENDERER_PEPPER_PEPPER_IN_PROCESS_RESOURCE_CREATION_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "ppapi/proxy/connection.h"
#include "webkit/plugins/ppapi/resource_creation_impl.h"

namespace content {

class RendererPpapiHostImpl;

// This class provides creation functions for the new resources with IPC
// backends that live in content/renderer/pepper.
//
// (See pepper_in_process_router.h for more information.)
//
// This is a bit confusing. The "old-style" resources live in
// webkit/plugins/ppapi and are created by the ResourceCreationImpl in that
// directory. The "new-style" IPC-only resources are in ppapi/proxy and are
// created by the RessourceCreationProxy in that directory.
//
// This class allows us to run new-style IPC-only resources in-process. We use
// the PepperInProcessRouter to run it in process. But then we have a problem
// with allocating the resources since src/webkit can't depend on IPC or see
// our IPC backend in content. This class overrides the normal in-process
// resource creation and adds in the resources that we implement in
// ppapi/proxy.
//
// When we convert all resources to use the new-style, we can just use the
// ResourceCreationProxy for all resources. This class is just glue to manage
// the temporary "two different cases."
class PepperInProcessResourceCreation
    : public webkit::ppapi::ResourceCreationImpl {
 public:
  PepperInProcessResourceCreation(RendererPpapiHostImpl* host_impl,
                                  webkit::ppapi::PluginInstance* instance);
  virtual ~PepperInProcessResourceCreation();

  // ResourceCreation_API implementation.
  virtual PP_Resource CreateBrowserFont(
      PP_Instance instance,
      const PP_BrowserFont_Trusted_Description* description) OVERRIDE;
  virtual PP_Resource CreateFileChooser(
      PP_Instance instance,
      PP_FileChooserMode_Dev mode,
      const char* accept_types) OVERRIDE;
  virtual PP_Resource CreateFileIO(PP_Instance instance) OVERRIDE;
  virtual PP_Resource CreateGraphics2D(
      PP_Instance pp_instance,
      const PP_Size& size,
      PP_Bool is_always_opaque) OVERRIDE;
  virtual PP_Resource CreatePrinting(
      PP_Instance instance) OVERRIDE;
  virtual PP_Resource CreateUDPSocketPrivate(PP_Instance instance) OVERRIDE;
  virtual PP_Resource CreateURLRequestInfo(
      PP_Instance instance,
      const ::ppapi::URLRequestInfoData& data) OVERRIDE;
  virtual PP_Resource CreateURLResponseInfo(
      PP_Instance instance,
      const ::ppapi::URLResponseInfoData& data,
      PP_Resource file_ref_resource) OVERRIDE;
  virtual PP_Resource CreateWebSocket(
      PP_Instance instance) OVERRIDE;

 private:
  // Non-owning pointer to the host for the current plugin.
  RendererPpapiHostImpl* host_impl_;

  DISALLOW_COPY_AND_ASSIGN(PepperInProcessResourceCreation);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_PEPPER_IN_PROCESS_RESOURCE_CREATION_H_
