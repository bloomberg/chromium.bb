// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/content_renderer_pepper_host_factory.h"

#include "base/logging.h"
#include "content/renderer/pepper/pepper_audio_input_host.h"
#include "content/renderer/pepper/pepper_file_chooser_host.h"
#include "content/renderer/pepper/pepper_flash_clipboard_host.h"
#include "content/renderer/pepper/pepper_graphics_2d_host.h"
#include "content/renderer/pepper/pepper_video_capture_host.h"
#include "content/renderer/pepper/pepper_websocket_host.h"
#include "content/renderer/pepper/renderer_ppapi_host_impl.h"
#include "ppapi/host/resource_host.h"
#include "ppapi/proxy/ppapi_messages.h"

using ppapi::host::ResourceHost;

namespace content {

ContentRendererPepperHostFactory::ContentRendererPepperHostFactory(
    RendererPpapiHostImpl* host)
    : host_(host) {
}

ContentRendererPepperHostFactory::~ContentRendererPepperHostFactory() {
}

scoped_ptr<ResourceHost> ContentRendererPepperHostFactory::CreateResourceHost(
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
    case PpapiHostMsg_Graphics2D_Create::ID: {
      PpapiHostMsg_Graphics2D_Create::Schema::Param msg_params;
      if (!PpapiHostMsg_Graphics2D_Create::Read(&message, &msg_params)) {
        NOTREACHED();
        return scoped_ptr<ResourceHost>();
      }
      return scoped_ptr<ResourceHost>(
          PepperGraphics2DHost::Create(host_, instance, params.pp_resource(),
                                       msg_params.a /* PP_Size */,
                                       msg_params.b /* PP_Bool */));
    }
    case PpapiHostMsg_WebSocket_Create::ID:
      return scoped_ptr<ResourceHost>(new PepperWebSocketHost(
          host_, instance, params.pp_resource()));
  }

  // Dev interfaces.
  if (GetPermissions().HasPermission(ppapi::PERMISSION_DEV)) {
    switch (message.type()) {
      case PpapiHostMsg_AudioInput_Create::ID:
        return scoped_ptr<ResourceHost>(new PepperAudioInputHost(
            host_, instance, params.pp_resource()));
      case PpapiHostMsg_FileChooser_Create::ID:
        return scoped_ptr<ResourceHost>(new PepperFileChooserHost(
            host_, instance, params.pp_resource()));
      case PpapiHostMsg_VideoCapture_Create::ID: {
        PepperVideoCaptureHost* host = new PepperVideoCaptureHost(
            host_, instance, params.pp_resource());
        if (!host->Init()) {
          delete host;
          return scoped_ptr<ResourceHost>();
        }
        return scoped_ptr<ResourceHost>(host);
      }
    }
  }

  // Flash interfaces.
  if (GetPermissions().HasPermission(ppapi::PERMISSION_FLASH)) {
    switch (message.type()) {
      case PpapiHostMsg_FlashClipboard_Create::ID:
        return scoped_ptr<ResourceHost>(new PepperFlashClipboardHost(
            host_, instance, params.pp_resource()));
    }
  }

  return scoped_ptr<ResourceHost>();
}

const ppapi::PpapiPermissions&
ContentRendererPepperHostFactory::GetPermissions() const {
  return host_->GetPpapiHost()->permissions();
}

}  // namespace content
