// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/pepper/pepper_flash_drm_host.h"

#if defined(OS_WIN)
#include <Windows.h>
#endif

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "content/public/browser/browser_ppapi_host.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/common/pepper_plugin_info.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/host/dispatch_host_message.h"
#include "ppapi/host/host_message_context.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/proxy/ppapi_messages.h"

using content::BrowserPpapiHost;

namespace chrome {

namespace {
const base::FilePath::CharType kVoucherFilename[] =
    FILE_PATH_LITERAL("plugin.vch");
}

PepperFlashDRMHost::PepperFlashDRMHost(BrowserPpapiHost* host,
                                       PP_Instance instance,
                                       PP_Resource resource)
    : ppapi::host::ResourceHost(host->GetPpapiHost(), instance, resource),
      weak_factory_(this){
  // Grant permissions to read the flash voucher file.
  int render_process_id, unused;
  bool success =
      host->GetRenderViewIDsForInstance(instance, &render_process_id, &unused);
  base::FilePath plugin_dir = host->GetPluginPath().DirName();
  DCHECK(!plugin_dir.empty() && success);
  base::FilePath voucher_file = plugin_dir.Append(
      base::FilePath(kVoucherFilename));
  content::ChildProcessSecurityPolicy::GetInstance()->GrantReadFile(
      render_process_id, voucher_file);

  // Init the DeviceIDFetcher.
  fetcher_ = new DeviceIDFetcher(render_process_id);
}

PepperFlashDRMHost::~PepperFlashDRMHost() {
}

int32_t PepperFlashDRMHost::OnResourceMessageReceived(
    const IPC::Message& msg,
    ppapi::host::HostMessageContext* context) {
  IPC_BEGIN_MESSAGE_MAP(PepperFlashDRMHost, msg)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(PpapiHostMsg_FlashDRM_GetDeviceID,
                                        OnHostMsgGetDeviceID)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(PpapiHostMsg_FlashDRM_GetHmonitor,
                                        OnHostMsgGetHmonitor)
  IPC_END_MESSAGE_MAP()
  return PP_ERROR_FAILED;
}

int32_t PepperFlashDRMHost::OnHostMsgGetDeviceID(
    ppapi::host::HostMessageContext* context) {
  if (!fetcher_->Start(base::Bind(&PepperFlashDRMHost::GotDeviceID,
                                  weak_factory_.GetWeakPtr(),
                                  context->MakeReplyMessageContext()))) {
    return PP_ERROR_INPROGRESS;
  }
  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperFlashDRMHost::OnHostMsgGetHmonitor(
    ppapi::host::HostMessageContext* context) {
#if defined(OS_WIN)
  // TODO(cpu): Get the real HMONITOR. See bug 249135.
  POINT pt = {1,1};
  HMONITOR monitor = ::MonitorFromPoint(pt, MONITOR_DEFAULTTOPRIMARY);
  int64_t monitor_id = reinterpret_cast<int64_t>(monitor);
  context->reply_msg = PpapiPluginMsg_FlashDRM_GetHmonitorReply(monitor_id);
  return PP_OK;
#else
  return PP_ERROR_FAILED;
#endif
}

void PepperFlashDRMHost::GotDeviceID(
    ppapi::host::ReplyMessageContext reply_context,
    const std::string& id) {
  reply_context.params.set_result(
      id.empty() ? PP_ERROR_FAILED : PP_OK);
  host()->SendReply(reply_context,
                    PpapiPluginMsg_FlashDRM_GetDeviceIDReply(id));
}

}  // namespace chrome
