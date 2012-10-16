// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/pepper_flash_host.h"

#include <vector>

#include "content/public/renderer/renderer_ppapi_host.h"
#include "ipc/ipc_message_macros.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/host/dispatch_host_message.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/proxy/enter_proxy.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/resource_message_params.h"
#include "ppapi/thunk/ppb_video_capture_api.h"

using ppapi::proxy::EnterHostFromHostResource;
using ppapi::proxy::EnterHostFromHostResourceForceCallback;
using ppapi::thunk::PPB_VideoCapture_API;

namespace content {

PepperFlashHost::PepperFlashHost(
    RendererPpapiHost* host,
    PP_Instance instance,
    PP_Resource resource)
    : ResourceHost(host->GetPpapiHost(), instance, resource),
      callback_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
}

PepperFlashHost::~PepperFlashHost() {
}

int32_t PepperFlashHost::OnResourceMessageReceived(
    const IPC::Message& msg,
    ppapi::host::HostMessageContext* context) {
  IPC_BEGIN_MESSAGE_MAP(PepperFlashHost, msg)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(
        PpapiHostMsg_Flash_EnumerateVideoCaptureDevices,
        OnMsgEnumerateVideoCaptureDevices)
  IPC_END_MESSAGE_MAP()
  return PP_ERROR_FAILED;
}

int32_t PepperFlashHost::OnMsgEnumerateVideoCaptureDevices(
    ppapi::host::HostMessageContext* host_context,
    const ppapi::HostResource& host_resource) {
  EnterHostFromHostResourceForceCallback<PPB_VideoCapture_API> enter(
      host_resource, callback_factory_,
      &PepperFlashHost::OnEnumerateVideoCaptureDevicesComplete,
      host_context->MakeReplyMessageContext(),
      host_resource);
  if (enter.succeeded()) {
    // We don't want the output to go into a PP_ResourceArray (which is
    // deprecated), so just pass in NULL. We'll grab the DeviceRefData vector
    // in the callback and convert it to a PP_ArrayOutput in the plugin.
    enter.SetResult(enter.object()->EnumerateDevices(NULL, enter.callback()));
  }
  return PP_OK_COMPLETIONPENDING;
}

void PepperFlashHost::OnEnumerateVideoCaptureDevicesComplete(
    int32_t result,
    ppapi::host::ReplyMessageContext reply_message_context,
    const ppapi::HostResource& host_resource) {
  std::vector<ppapi::DeviceRefData> devices;
  if (result == PP_OK) {
    EnterHostFromHostResource<PPB_VideoCapture_API> enter(host_resource);
    if (enter.succeeded())
      devices = enter.object()->GetDeviceRefData();
    else
      result = PP_ERROR_FAILED;
  }
  reply_message_context.params.set_result(result);
  host()->SendReply(reply_message_context,
      PpapiPluginMsg_Flash_EnumerateVideoCaptureDevicesReply(devices));
}

}  // namespace content

