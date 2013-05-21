// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/pepper/pepper_flash_device_id_host.h"

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "content/public/browser/browser_ppapi_host.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/host/dispatch_host_message.h"
#include "ppapi/host/host_message_context.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/proxy/ppapi_messages.h"

using content::BrowserPpapiHost;

namespace chrome {

PepperFlashDeviceIDHost::PepperFlashDeviceIDHost(BrowserPpapiHost* host,
                                                 PP_Instance instance,
                                                 PP_Resource resource)
    : ppapi::host::ResourceHost(host->GetPpapiHost(), instance, resource),
      weak_factory_(this){
  int render_process_id, unused;
  host->GetRenderViewIDsForInstance(instance, &render_process_id, &unused);
  fetcher_ = new DeviceIDFetcher(render_process_id);
}

PepperFlashDeviceIDHost::~PepperFlashDeviceIDHost() {
}

int32_t PepperFlashDeviceIDHost::OnResourceMessageReceived(
    const IPC::Message& msg,
    ppapi::host::HostMessageContext* context) {
  IPC_BEGIN_MESSAGE_MAP(PepperFlashDeviceIDHost, msg)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(PpapiHostMsg_FlashDeviceID_GetDeviceID,
                                        OnHostMsgGetDeviceID)
  IPC_END_MESSAGE_MAP()
  return PP_ERROR_FAILED;
}

int32_t PepperFlashDeviceIDHost::OnHostMsgGetDeviceID(
    const ppapi::host::HostMessageContext* context) {
  if (!fetcher_->Start(base::Bind(&PepperFlashDeviceIDHost::GotDeviceID,
                                  weak_factory_.GetWeakPtr(),
                                  context->MakeReplyMessageContext()))) {
    return PP_ERROR_INPROGRESS;
  }
  return PP_OK_COMPLETIONPENDING;
}

void PepperFlashDeviceIDHost::GotDeviceID(
    ppapi::host::ReplyMessageContext reply_context,
    const std::string& id) {
  reply_context.params.set_result(
      id.empty() ? PP_ERROR_FAILED : PP_OK);
  host()->SendReply(reply_context,
                    PpapiPluginMsg_FlashDeviceID_GetDeviceIDReply(id));
}

}  // namespace chrome
