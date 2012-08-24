// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/pepper/pepper_gamepad_host.h"

#include "content/public/browser/browser_ppapi_host.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/host/dispatch_host_message.h"
#include "ppapi/proxy/ppapi_messages.h"

namespace content {

PepperGamepadHost::PepperGamepadHost(BrowserPpapiHost* host,
                                     PP_Instance instance,
                                     PP_Resource resource)
    : ResourceHost(host->GetPpapiHost(), instance, resource),
      browser_ppapi_host_(host) {
}

PepperGamepadHost::~PepperGamepadHost() {
}

int32_t PepperGamepadHost::OnResourceMessageReceived(
    const IPC::Message& msg,
    ppapi::host::HostMessageContext* context) {
  IPC_BEGIN_MESSAGE_MAP(PepperGamepadHost, msg)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(PpapiHostMsg_Gamepad_RequestMemory,
                                        OnMsgRequestMemory)
  IPC_END_MESSAGE_MAP()
  return PP_ERROR_FAILED;
}

int32_t PepperGamepadHost::OnMsgRequestMemory(
    ppapi::host::HostMessageContext* context) {
  return PP_ERROR_FAILED;
}

}  // namespace content
