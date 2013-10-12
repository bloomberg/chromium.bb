// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/pepper/pepper_output_protection_message_filter.h"

#include "build/build_config.h"
#include "content/public/browser/browser_ppapi_host.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/private/ppb_output_protection_private.h"
#include "ppapi/host/dispatch_host_message.h"
#include "ppapi/host/host_message_context.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/proxy/ppapi_messages.h"

#if defined(USE_ASH) && defined(OS_CHROMEOS)
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "chromeos/display/output_configurator.h"
#endif

namespace chrome {

namespace {

#if defined(OS_CHROMEOS)
COMPILE_ASSERT(
    static_cast<int>(PP_OUTPUT_PROTECTION_LINK_TYPE_PRIVATE_NONE) ==
    static_cast<int>(chromeos::OUTPUT_TYPE_NONE),
    PP_OUTPUT_PROTECTION_LINK_TYPE_PRIVATE_NONE);
COMPILE_ASSERT(
    static_cast<int>(PP_OUTPUT_PROTECTION_LINK_TYPE_PRIVATE_UNKNOWN) ==
    static_cast<int>(chromeos::OUTPUT_TYPE_UNKNOWN),
    PP_OUTPUT_PROTECTION_LINK_TYPE_PRIVATE_UNKNOWN);
COMPILE_ASSERT(
    static_cast<int>(PP_OUTPUT_PROTECTION_LINK_TYPE_PRIVATE_INTERNAL) ==
    static_cast<int>(chromeos::OUTPUT_TYPE_INTERNAL),
    PP_OUTPUT_PROTECTION_LINK_TYPE_PRIVATE_INTERNAL);
COMPILE_ASSERT(
    static_cast<int>(PP_OUTPUT_PROTECTION_LINK_TYPE_PRIVATE_VGA) ==
    static_cast<int>(chromeos::OUTPUT_TYPE_VGA),
    PP_OUTPUT_PROTECTION_LINK_TYPE_PRIVATE_VGA);
COMPILE_ASSERT(
    static_cast<int>(PP_OUTPUT_PROTECTION_LINK_TYPE_PRIVATE_HDMI) ==
    static_cast<int>(chromeos::OUTPUT_TYPE_HDMI),
    PP_OUTPUT_PROTECTION_LINK_TYPE_PRIVATE_HDMI);
COMPILE_ASSERT(
    static_cast<int>(PP_OUTPUT_PROTECTION_LINK_TYPE_PRIVATE_DVI) ==
    static_cast<int>(chromeos::OUTPUT_TYPE_DVI),
    PP_OUTPUT_PROTECTION_LINK_TYPE_PRIVATE_DVI);
COMPILE_ASSERT(
    static_cast<int>(PP_OUTPUT_PROTECTION_LINK_TYPE_PRIVATE_DISPLAYPORT) ==
    static_cast<int>(chromeos::OUTPUT_TYPE_DISPLAYPORT),
    PP_OUTPUT_PROTECTION_LINK_TYPE_PRIVATE_DISPLAYPORT);
COMPILE_ASSERT(
    static_cast<int>(PP_OUTPUT_PROTECTION_LINK_TYPE_PRIVATE_NETWORK) ==
    static_cast<int>(chromeos::OUTPUT_TYPE_NETWORK),
    PP_OUTPUT_PROTECTION_LINK_TYPE_PRIVATE_NETWORK);
COMPILE_ASSERT(
    static_cast<int>(PP_OUTPUT_PROTECTION_METHOD_PRIVATE_NONE) ==
    static_cast<int>(chromeos::OUTPUT_PROTECTION_METHOD_NONE),
    PP_OUTPUT_PROTECTION_METHOD_PRIVATE_NONE);
COMPILE_ASSERT(
    static_cast<int>(PP_OUTPUT_PROTECTION_METHOD_PRIVATE_HDCP) ==
    static_cast<int>(chromeos::OUTPUT_PROTECTION_METHOD_HDCP),
    PP_OUTPUT_PROTECTION_METHOD_PRIVATE_HDCP);
#endif

#if defined(OS_CHROMEOS) && defined(USE_ASH)
void UnregisterClientOnUIThread(
    chromeos::OutputConfigurator::OutputProtectionClientId client_id) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  chromeos::OutputConfigurator* configurator =
      ash::Shell::GetInstance()->output_configurator();
  configurator->UnregisterOutputProtectionClient(client_id);
}
#endif

}  // namespace

PepperOutputProtectionMessageFilter::PepperOutputProtectionMessageFilter(
    content::BrowserPpapiHost* host,
    PP_Instance instance) {
#if defined(OS_CHROMEOS) && defined(USE_ASH) && defined(USE_X11)
  client_id_ = 0;
  host->GetRenderViewIDsForInstance(
      instance, &render_process_id_, &render_view_id_);
#else
  NOTIMPLEMENTED();
#endif
}

PepperOutputProtectionMessageFilter::~PepperOutputProtectionMessageFilter() {
#if defined(OS_CHROMEOS) && defined(USE_ASH)
  if (client_id_ != 0) {
    content::BrowserThread::PostTask(
        content::BrowserThread::UI,
        FROM_HERE,
        base::Bind(&UnregisterClientOnUIThread, client_id_));
  }
#endif
}

scoped_refptr<base::TaskRunner>
PepperOutputProtectionMessageFilter::OverrideTaskRunnerForMessage(
    const IPC::Message& message) {
  return content::BrowserThread::GetMessageLoopProxyForThread(
      content::BrowserThread::UI);
}

int32_t PepperOutputProtectionMessageFilter::OnResourceMessageReceived(
    const IPC::Message& msg,
    ppapi::host::HostMessageContext* context) {
  IPC_BEGIN_MESSAGE_MAP(PepperOutputProtectionMessageFilter, msg)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(
        PpapiHostMsg_OutputProtection_QueryStatus,
        OnQueryStatus);
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(
        PpapiHostMsg_OutputProtection_EnableProtection,
        OnEnableProtection);
  IPC_END_MESSAGE_MAP()
  return PP_ERROR_FAILED;
}

#if defined(OS_CHROMEOS)
chromeos::OutputConfigurator::OutputProtectionClientId
PepperOutputProtectionMessageFilter::GetClientId() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (client_id_ == 0) {
#if defined(USE_ASH) && defined(USE_X11)
    chromeos::OutputConfigurator* configurator =
        ash::Shell::GetInstance()->output_configurator();
    client_id_ = configurator->RegisterOutputProtectionClient();
#endif
  }
  return client_id_;
}
#endif

int32_t PepperOutputProtectionMessageFilter::OnQueryStatus(
    ppapi::host::HostMessageContext* context) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

#if defined(OS_CHROMEOS) && defined(USE_ASH) && defined(USE_X11)
  ppapi::host::ReplyMessageContext reply_context =
      context->MakeReplyMessageContext();
  uint32_t link_mask = 0, protection_mask = 0;
  chromeos::OutputConfigurator* configurator =
      ash::Shell::GetInstance()->output_configurator();
  bool result = configurator->QueryOutputProtectionStatus(
      GetClientId(), &link_mask, &protection_mask);

  // If we successfully retrieved the device level status, check for capturers.
  if (result) {
    // Ensure the RenderViewHost is still alive.
    content::RenderViewHost* rvh =
        content::RenderViewHost::FromID(render_process_id_, render_view_id_);
    if (rvh) {
      if (content::WebContents::FromRenderViewHost(rvh)->GetCapturerCount() > 0)
        link_mask |= chromeos::OUTPUT_TYPE_NETWORK;
    }
  }

  reply_context.params.set_result(result ? PP_OK : PP_ERROR_FAILED);
  SendReply(
      reply_context,
      PpapiPluginMsg_OutputProtection_QueryStatusReply(
          link_mask, protection_mask));
  return PP_OK_COMPLETIONPENDING;
#else
  NOTIMPLEMENTED();
  return PP_ERROR_NOTSUPPORTED;
#endif
}

int32_t PepperOutputProtectionMessageFilter::OnEnableProtection(
    ppapi::host::HostMessageContext* context,
    uint32_t desired_method_mask) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

#if defined(OS_CHROMEOS) && defined(USE_ASH) && defined(USE_X11)
  ppapi::host::ReplyMessageContext reply_context =
      context->MakeReplyMessageContext();
  chromeos::OutputConfigurator* configurator =
      ash::Shell::GetInstance()->output_configurator();
  bool result = configurator->EnableOutputProtection(
      GetClientId(), desired_method_mask);
  reply_context.params.set_result(result ? PP_OK : PP_ERROR_FAILED);
  SendReply(
      reply_context,
      PpapiPluginMsg_OutputProtection_EnableProtectionReply());
  return PP_OK_COMPLETIONPENDING;
#else
  NOTIMPLEMENTED();
  return PP_ERROR_NOTSUPPORTED;
#endif
}

}  // namespace chrome

