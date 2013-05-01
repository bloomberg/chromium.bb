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
      factory_(this),
      browser_ppapi_host_(host),
      in_progress_(false),
      weak_factory_(this){
  fetcher_ = new DeviceIDFetcher(
      base::Bind(&PepperFlashDeviceIDHost::GotDeviceID,
                 weak_factory_.GetWeakPtr()), instance);
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
  if (in_progress_)
    return PP_ERROR_INPROGRESS;

  in_progress_ = true;
  fetcher_->Start(browser_ppapi_host_);
  reply_context_ = context->MakeReplyMessageContext();
  return PP_OK_COMPLETIONPENDING;
}

void PepperFlashDeviceIDHost::GotDeviceID(const std::string& id) {
  in_progress_ = false;
  reply_context_.params.set_result(
      id.empty() ? PP_ERROR_FAILED : PP_OK);
  host()->SendReply(reply_context_,
                    PpapiPluginMsg_FlashDeviceID_GetDeviceIDReply(id));
}

}  // namespace chrome
