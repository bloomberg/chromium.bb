// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/pepper/content_browser_pepper_host_factory.h"

#include "content/browser/renderer_host/pepper/browser_ppapi_host_impl.h"
#include "content/browser/renderer_host/pepper/pepper_browser_font_singleton_host.h"
#include "content/browser/renderer_host/pepper/pepper_file_ref_host.h"
#include "content/browser/renderer_host/pepper/pepper_file_system_browser_host.h"
#include "content/browser/renderer_host/pepper/pepper_flash_file_message_filter.h"
#include "content/browser/renderer_host/pepper/pepper_gamepad_host.h"
#include "content/browser/renderer_host/pepper/pepper_host_resolver_message_filter.h"
#include "content/browser/renderer_host/pepper/pepper_network_proxy_host.h"
#include "content/browser/renderer_host/pepper/pepper_print_settings_manager.h"
#include "content/browser/renderer_host/pepper/pepper_printing_host.h"
#include "content/browser/renderer_host/pepper/pepper_tcp_server_socket_message_filter.h"
#include "content/browser/renderer_host/pepper/pepper_truetype_font_list_host.h"
#include "content/browser/renderer_host/pepper/pepper_udp_socket_message_filter.h"
#include "ppapi/host/message_filter_host.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/host/resource_host.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/ppapi_permissions.h"

using ppapi::host::MessageFilterHost;
using ppapi::host::ResourceHost;
using ppapi::host::ResourceMessageFilter;
using ppapi::UnpackMessage;

namespace content {

namespace {

const size_t kMaxSocketsAllowed = 1024;

bool CanCreateSocket() {
  return
      PepperUDPSocketMessageFilter::GetNumInstances() +
      PepperTCPServerSocketMessageFilter::GetNumInstances() <
      kMaxSocketsAllowed;
}

}  // namespace

ContentBrowserPepperHostFactory::ContentBrowserPepperHostFactory(
    BrowserPpapiHostImpl* host,
    const scoped_refptr<PepperMessageFilter>& pepper_message_filter)
    : host_(host),
      pepper_message_filter_(pepper_message_filter) {
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
    case PpapiHostMsg_FileSystem_Create::ID: {
      PP_FileSystemType file_system_type;
      if (!ppapi::UnpackMessage<PpapiHostMsg_FileSystem_Create>(message,
          &file_system_type)) {
        NOTREACHED();
        return scoped_ptr<ResourceHost>();
      }
      return scoped_ptr<ResourceHost>(new PepperFileSystemBrowserHost(
          host_, instance, params.pp_resource(), file_system_type));
    }
    case PpapiHostMsg_Gamepad_Create::ID: {
      return scoped_ptr<ResourceHost>(new PepperGamepadHost(
          host_, instance, params.pp_resource()));
    }
    case PpapiHostMsg_NetworkProxy_Create::ID: {
      return scoped_ptr<ResourceHost>(new PepperNetworkProxyHost(
          host_, instance, params.pp_resource()));
    }
    case PpapiHostMsg_HostResolver_Create::ID: {
      scoped_refptr<ResourceMessageFilter> host_resolver(
          new PepperHostResolverMessageFilter(host_, instance, false));
      return scoped_ptr<ResourceHost>(new MessageFilterHost(
          host_->GetPpapiHost(), instance, params.pp_resource(),
          host_resolver));
    }
    case PpapiHostMsg_FileRef_CreateInternal::ID: {
      PP_Resource file_system;
      std::string internal_path;
      if (!UnpackMessage<PpapiHostMsg_FileRef_CreateInternal>(
          message, &file_system, &internal_path)) {
        NOTREACHED();
        return scoped_ptr<ResourceHost>();
      }
      return scoped_ptr<ResourceHost>(new PepperFileRefHost(
          host_, instance, params.pp_resource(), file_system, internal_path));
    }
    case PpapiHostMsg_UDPSocket_Create::ID: {
      if (CanCreateSocket()) {
        scoped_refptr<ResourceMessageFilter> udp_socket(
            new PepperUDPSocketMessageFilter(host_, instance, false));
        return scoped_ptr<ResourceHost>(new MessageFilterHost(
            host_->GetPpapiHost(), instance, params.pp_resource(), udp_socket));
      } else {
        return scoped_ptr<ResourceHost>();
      }
    }
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
      case PpapiHostMsg_TrueTypeFontSingleton_Create::ID: {
        return scoped_ptr<ResourceHost>(new PepperTrueTypeFontListHost(
            host_, instance, params.pp_resource()));
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
  // thread). Currently these interfaces are available only for
  // whitelisted apps which may not have access to the other private
  // interfaces.
  if (message.type() == PpapiHostMsg_HostResolver_CreatePrivate::ID) {
    scoped_refptr<ResourceMessageFilter> host_resolver(
        new PepperHostResolverMessageFilter(host_, instance, true));
    return scoped_ptr<ResourceHost>(new MessageFilterHost(
        host_->GetPpapiHost(), instance, params.pp_resource(), host_resolver));
  }
  if (message.type() == PpapiHostMsg_TCPServerSocket_CreatePrivate::ID) {
    if (CanCreateSocket()) {
      scoped_refptr<ResourceMessageFilter> tcp_server_socket(
          new PepperTCPServerSocketMessageFilter(host_, instance, true,
                                                 pepper_message_filter_));
      return scoped_ptr<ResourceHost>(new MessageFilterHost(
          host_->GetPpapiHost(), instance, params.pp_resource(),
          tcp_server_socket));
    } else {
      return scoped_ptr<ResourceHost>();
    }
  }
  if (message.type() == PpapiHostMsg_UDPSocket_CreatePrivate::ID) {
    if (CanCreateSocket()) {
      scoped_refptr<ResourceMessageFilter> udp_socket(
          new PepperUDPSocketMessageFilter(host_, instance, true));
      return scoped_ptr<ResourceHost>(new MessageFilterHost(
          host_->GetPpapiHost(), instance, params.pp_resource(), udp_socket));
    } else {
      return scoped_ptr<ResourceHost>();
    }
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
