// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/pepper/chrome_renderer_pepper_host_factory.h"

#include "base/logging.h"
#include "chrome/renderer/pepper/pepper_extensions_common_host.h"
#include "chrome/renderer/pepper/pepper_flash_font_file_host.h"
#include "chrome/renderer/pepper/pepper_flash_fullscreen_host.h"
#include "chrome/renderer/pepper/pepper_flash_menu_host.h"
#include "chrome/renderer/pepper/pepper_flash_renderer_host.h"
#include "content/public/renderer/renderer_ppapi_host.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/host/resource_host.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/ppapi_message_utils.h"
#include "ppapi/shared_impl/ppapi_permissions.h"

using ppapi::host::ResourceHost;

namespace chrome {

ChromeRendererPepperHostFactory::ChromeRendererPepperHostFactory(
    content::RendererPpapiHost* host)
    : host_(host) {
}

ChromeRendererPepperHostFactory::~ChromeRendererPepperHostFactory() {
}

scoped_ptr<ResourceHost>
ChromeRendererPepperHostFactory::CreateResourceHost(
    ppapi::host::PpapiHost* host,
    const ppapi::proxy::ResourceMessageCallParams& params,
    PP_Instance instance,
    const IPC::Message& message) {
  DCHECK(host == host_->GetPpapiHost());

  // Make sure the plugin is giving us a valid instance for this resource.
  if (!host_->IsValidInstance(instance))
    return scoped_ptr<ResourceHost>();

  // Dev interfaces.
  if (host_->GetPpapiHost()->permissions().HasPermission(
      ppapi::PERMISSION_DEV)) {
    switch (message.type()) {
      case PpapiHostMsg_ExtensionsCommon_Create::ID: {
        return scoped_ptr<ResourceHost>(PepperExtensionsCommonHost::Create(
            host_, instance, params.pp_resource()));
      }
    }
  }

  if (host_->GetPpapiHost()->permissions().HasPermission(
      ppapi::PERMISSION_FLASH)) {
    switch (message.type()) {
      case PpapiHostMsg_Flash_Create::ID: {
        return scoped_ptr<ResourceHost>(new PepperFlashRendererHost(
            host_, instance, params.pp_resource()));
      }
      case PpapiHostMsg_FlashFullscreen_Create::ID: {
        return scoped_ptr<ResourceHost>(new PepperFlashFullscreenHost(
            host_, instance, params.pp_resource()));
      }
      case PpapiHostMsg_FlashMenu_Create::ID: {
        ppapi::proxy::SerializedFlashMenu serialized_menu;
        if (ppapi::UnpackMessage<PpapiHostMsg_FlashMenu_Create>(
            message, &serialized_menu)) {
          return scoped_ptr<ResourceHost>(new PepperFlashMenuHost(
              host_, instance, params.pp_resource(), serialized_menu));
        }
        break;
      }
    }
  }

  // TODO(raymes): PDF also needs access to the FlashFontFileHost currently.
  // We should either rename PPB_FlashFont_File to PPB_FontFile_Private or get
  // rid of its use in PDF if possible.
  if (host_->GetPpapiHost()->permissions().HasPermission(
          ppapi::PERMISSION_FLASH) ||
      host_->GetPpapiHost()->permissions().HasPermission(
          ppapi::PERMISSION_PRIVATE)) {
    switch (message.type()) {
      case PpapiHostMsg_FlashFontFile_Create::ID: {
        ppapi::proxy::SerializedFontDescription description;
        PP_PrivateFontCharset charset;
        if (ppapi::UnpackMessage<PpapiHostMsg_FlashFontFile_Create>(
            message, &description, &charset)) {
          return scoped_ptr<ResourceHost>(new PepperFlashFontFileHost(
              host_, instance, params.pp_resource(), description, charset));
        }
        break;
      }
    }
  }

  return scoped_ptr<ResourceHost>();
}

}  // namespace chrome
