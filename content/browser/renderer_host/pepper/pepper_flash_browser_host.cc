// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/pepper/pepper_flash_browser_host.h"

#include "content/public/browser/browser_ppapi_host.h"
#include "ipc/ipc_message_macros.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/host/dispatch_host_message.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/resource_message_params.h"

#ifdef OS_WIN
#include <windows.h>
#elif defined(OS_MACOSX)
#include <CoreServices/CoreServices.h>
#endif

namespace content {

PepperFlashBrowserHost::PepperFlashBrowserHost(
    BrowserPpapiHost* host,
    PP_Instance instance,
    PP_Resource resource)
    : ResourceHost(host->GetPpapiHost(), instance, resource) {
}

PepperFlashBrowserHost::~PepperFlashBrowserHost() {
}

int32_t PepperFlashBrowserHost::OnResourceMessageReceived(
    const IPC::Message& msg,
    ppapi::host::HostMessageContext* context) {
  IPC_BEGIN_MESSAGE_MAP(PepperFlashBrowserHost, msg)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(PpapiHostMsg_Flash_UpdateActivity,
                                        OnMsgUpdateActivity);
  IPC_END_MESSAGE_MAP()
  return PP_ERROR_FAILED;
}

int32_t PepperFlashBrowserHost::OnMsgUpdateActivity(
    ppapi::host::HostMessageContext* host_context) {
#if defined(OS_WIN)
  // Reading then writing back the same value to the screensaver timeout system
  // setting resets the countdown which prevents the screensaver from turning
  // on "for a while". As long as the plugin pings us with this message faster
  // than the screensaver timeout, it won't go on.
  int value = 0;
  if (SystemParametersInfo(SPI_GETSCREENSAVETIMEOUT, 0, &value, 0))
    SystemParametersInfo(SPI_SETSCREENSAVETIMEOUT, value, NULL, 0);
#elif defined(OS_MACOSX)
  UpdateSystemActivity(OverallAct);
#else
  // TODO(brettw) implement this for other platforms.
#endif
  return PP_OK;
}

}  // namespace content
