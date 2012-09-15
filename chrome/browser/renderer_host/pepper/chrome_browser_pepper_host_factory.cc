// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/pepper/chrome_browser_pepper_host_factory.h"

#include "chrome/browser/renderer_host/pepper/pepper_flash_device_id_host.h"
#include "content/public/browser/browser_ppapi_host.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/host/resource_host.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/ppapi_permissions.h"

using ppapi::host::ResourceHost;

namespace chrome {

ChromeBrowserPepperHostFactory::ChromeBrowserPepperHostFactory(
    content::BrowserPpapiHost* host)
    : host_(host) {
}

ChromeBrowserPepperHostFactory::~ChromeBrowserPepperHostFactory() {
}

scoped_ptr<ResourceHost> ChromeBrowserPepperHostFactory::CreateResourceHost(
    ppapi::host::PpapiHost* host,
    const ppapi::proxy::ResourceMessageCallParams& params,
    PP_Instance instance,
    const IPC::Message& message) {
  DCHECK(host == host_->GetPpapiHost());

  // Make sure the plugin is giving us a valid instance for this resource.
  if (!host_->IsValidInstance(instance))
    return scoped_ptr<ResourceHost>();

  // Private interfaces.
  if (host_->GetPpapiHost()->permissions().HasPermission(
          ppapi::PERMISSION_PRIVATE)) {
    switch (message.type()) {
      case PpapiHostMsg_FlashDeviceID_Create::ID:
        return scoped_ptr<ResourceHost>(new PepperFlashDeviceIDHost(
            host_, instance, params.pp_resource()));
    }
  }
  return scoped_ptr<ResourceHost>();
}

}  // namespace chrome
