// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/pepper/content_browser_pepper_host_factory.h"

#include "content/browser/renderer_host/pepper/browser_ppapi_host_impl.h"
#include "content/browser/renderer_host/pepper/pepper_browser_font_singleton_host.h"
#include "content/browser/renderer_host/pepper/pepper_flash_file_message_filter.h"
#include "content/browser/renderer_host/pepper/pepper_gamepad_host.h"
#include "content/browser/renderer_host/pepper/pepper_host_resolver_private_message_filter.h"
#include "content/browser/renderer_host/pepper/pepper_print_settings_manager.h"
#include "content/browser/renderer_host/pepper/pepper_printing_host.h"
#include "content/browser/renderer_host/pepper/pepper_udp_socket_private_host.h"
#include "ppapi/host/message_filter_host.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/host/resource_host.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/ppapi_permissions.h"

using ppapi::host::MessageFilterHost;
using ppapi::host::ResourceHost;
using ppapi::host::ResourceMessageFilter;

namespace content {

ContentBrowserPepperHostFactory::ContentBrowserPepperHostFactory(
    BrowserPpapiHostImpl* host)
    : host_(host) {
}

ContentBrowserPepperHostFactory::~ContentBrowserPepperHostFactory() {
}

scoped_ptr<ResourceHost> ContentBrowserPepperHostFactory::CreateResourceHost(
    ppapi::host::PpapiHost* host,
    const ppapi::proxy::ResourceMessageCallParams& params,
    PP_Instance instance,
    const IPC::Message& message) {
  DCHECK(host == host_->GetPpapiHost());

  // Make sure the plugin is giving us a valid instance for this resource.
  if (!host_->IsValidInstance(instance))
    return scoped_ptr<ResourceHost>();

  // Public interfaces.
  switch (message.type()) {
    case PpapiHostMsg_Gamepad_Create::ID:
      return scoped_ptr<ResourceHost>(new PepperGamepadHost(
          host_, instance, params.pp_resource()));
  }

  // Dev interfaces.
  if (GetPermissions().HasPermission(ppapi::PERMISSION_DEV)) {
    switch (message.type()) {
      case PpapiHostMsg_Printing_Create::ID: {
         scoped_ptr<PepperPrintSettingsManager> manager(
             new PepperPrintSettingsManagerImpl());
         return scoped_ptr<ResourceHost>(new PepperPrintingHost(
             host_->GetPpapiHost(), instance,
             params.pp_resource(), manager.Pass()));
      }
    }
  }

  // Private interfaces.
  if (GetPermissions().HasPermission(ppapi::PERMISSION_PRIVATE)) {
    switch (message.type()) {
      case PpapiHostMsg_BrowserFontSingleton_Create::ID:
        return scoped_ptr<ResourceHost>(new PepperBrowserFontSingletonHost(
            host_, instance, params.pp_resource()));
    }
  }

  // Permissions for the following interfaces will be checked at the
  // time of the corresponding instance's methods calls (because
  // permission check can be performed only on the UI
  // thread). Currently thise interfaces are available only for
  // whitelisted apps which may not have access to the other private
  // interfaces.
  if (message.type() == PpapiHostMsg_HostResolverPrivate_Create::ID) {
    scoped_refptr<ResourceMessageFilter> host_resolver(
        new PepperHostResolverPrivateMessageFilter(host_, instance));
    return scoped_ptr<ResourceHost>(new MessageFilterHost(
        host_->GetPpapiHost(), instance, params.pp_resource(), host_resolver));
  }
  if (message.type() == PpapiHostMsg_UDPSocketPrivate_Create::ID) {
    return scoped_ptr<ResourceHost>(new PepperUDPSocketPrivateHost(
        host_, instance, params.pp_resource()));
  }

  // Flash interfaces.
  if (GetPermissions().HasPermission(ppapi::PERMISSION_FLASH)) {
    switch (message.type()) {
      case PpapiHostMsg_FlashFile_Create::ID: {
        scoped_refptr<ResourceMessageFilter> file_filter(
            new PepperFlashFileMessageFilter(instance, host_));
        return scoped_ptr<ResourceHost>(new MessageFilterHost(
            host_->GetPpapiHost(), instance, params.pp_resource(),
            file_filter));
      }
    }
  }

  return scoped_ptr<ResourceHost>();
}

const ppapi::PpapiPermissions&
ContentBrowserPepperHostFactory::GetPermissions() const {
  return host_->GetPpapiHost()->permissions();
}

}  // namespace content
