// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/pepper/chrome_browser_pepper_host_factory.h"

#include "build/build_config.h"
#include "chrome/browser/renderer_host/pepper/pepper_broker_message_filter.h"
#include "chrome/browser/renderer_host/pepper/pepper_flash_browser_host.h"
#include "chrome/browser/renderer_host/pepper/pepper_flash_clipboard_message_filter.h"
#include "chrome/browser/renderer_host/pepper/pepper_flash_drm_host.h"
#include "chrome/browser/renderer_host/pepper/pepper_isolated_file_system_message_filter.h"
#include "chrome/browser/renderer_host/pepper/pepper_output_protection_message_filter.h"
#include "chrome/browser/renderer_host/pepper/pepper_platform_verification_message_filter.h"
#include "chrome/browser/renderer_host/pepper/pepper_talk_host.h"
#include "content/public/browser/browser_ppapi_host.h"
#include "ppapi/host/message_filter_host.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/host/resource_host.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/ppapi_permissions.h"

using ppapi::host::MessageFilterHost;
using ppapi::host::ResourceHost;
using ppapi::host::ResourceMessageFilter;

namespace chrome {

ChromeBrowserPepperHostFactory::ChromeBrowserPepperHostFactory(
    content::BrowserPpapiHost* host)
    : host_(host) {}

ChromeBrowserPepperHostFactory::~ChromeBrowserPepperHostFactory() {}

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
      case PpapiHostMsg_Broker_Create::ID: {
        scoped_refptr<ResourceMessageFilter> broker_filter(
            new PepperBrokerMessageFilter(instance, host_));
        return scoped_ptr<ResourceHost>(
            new MessageFilterHost(host_->GetPpapiHost(),
                                  instance,
                                  params.pp_resource(),
                                  broker_filter));
      }
#if defined(OS_CHROMEOS)
      case PpapiHostMsg_PlatformVerification_Create::ID: {
        scoped_refptr<ResourceMessageFilter> pv_filter(
            new PepperPlatformVerificationMessageFilter(host_, instance));
        return scoped_ptr<ResourceHost>(new MessageFilterHost(
            host_->GetPpapiHost(), instance, params.pp_resource(), pv_filter));
      }
#endif
      case PpapiHostMsg_Talk_Create::ID:
        return scoped_ptr<ResourceHost>(
            new PepperTalkHost(host_, instance, params.pp_resource()));
#if defined(OS_CHROMEOS)
      case PpapiHostMsg_OutputProtection_Create::ID: {
        scoped_refptr<ResourceMessageFilter> output_protection_filter(
            new PepperOutputProtectionMessageFilter(host_, instance));
        return scoped_ptr<ResourceHost>(
            new MessageFilterHost(host_->GetPpapiHost(),
                                  instance,
                                  params.pp_resource(),
                                  output_protection_filter));
      }
#endif
    }
  }

  // Flash interfaces.
  if (host_->GetPpapiHost()->permissions().HasPermission(
          ppapi::PERMISSION_FLASH)) {
    switch (message.type()) {
      case PpapiHostMsg_Flash_Create::ID:
        return scoped_ptr<ResourceHost>(
            new PepperFlashBrowserHost(host_, instance, params.pp_resource()));
      case PpapiHostMsg_FlashClipboard_Create::ID: {
        scoped_refptr<ResourceMessageFilter> clipboard_filter(
            new PepperFlashClipboardMessageFilter);
        return scoped_ptr<ResourceHost>(
            new MessageFilterHost(host_->GetPpapiHost(),
                                  instance,
                                  params.pp_resource(),
                                  clipboard_filter));
      }
      case PpapiHostMsg_FlashDRM_Create::ID:
        return scoped_ptr<ResourceHost>(
            new PepperFlashDRMHost(host_, instance, params.pp_resource()));
    }
  }

  // Permissions for the following interfaces will be checked at the
  // time of the corresponding instance's methods calls (because
  // permission check can be performed only on the UI
  // thread). Currently these interfaces are available only for
  // whitelisted apps which may not have access to the other private
  // interfaces.
  if (message.type() == PpapiHostMsg_IsolatedFileSystem_Create::ID) {
    PepperIsolatedFileSystemMessageFilter* isolated_fs_filter =
        PepperIsolatedFileSystemMessageFilter::Create(instance, host_);
    if (!isolated_fs_filter)
      return scoped_ptr<ResourceHost>();
    return scoped_ptr<ResourceHost>(new MessageFilterHost(
        host, instance, params.pp_resource(), isolated_fs_filter));
  }

  return scoped_ptr<ResourceHost>();
}

}  // namespace chrome
