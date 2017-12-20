// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/pepper/pepper_output_protection_message_filter.h"

#include "build/build_config.h"
#include "chrome/browser/media/output_protection_proxy.h"
#include "content/public/browser/browser_ppapi_host.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/private/ppb_output_protection_private.h"
#include "ppapi/host/dispatch_host_message.h"
#include "ppapi/host/host_message_context.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ui/display/types/display_constants.h"

namespace {

static_assert(static_cast<int>(PP_OUTPUT_PROTECTION_LINK_TYPE_PRIVATE_NONE) ==
                  static_cast<int>(display::DISPLAY_CONNECTION_TYPE_NONE),
              "PP_OUTPUT_PROTECTION_LINK_TYPE_PRIVATE_NONE value mismatch");
static_assert(
    static_cast<int>(PP_OUTPUT_PROTECTION_LINK_TYPE_PRIVATE_UNKNOWN) ==
        static_cast<int>(display::DISPLAY_CONNECTION_TYPE_UNKNOWN),
    "PP_OUTPUT_PROTECTION_LINK_TYPE_PRIVATE_UNKNOWN value mismatch");
static_assert(
    static_cast<int>(PP_OUTPUT_PROTECTION_LINK_TYPE_PRIVATE_INTERNAL) ==
        static_cast<int>(display::DISPLAY_CONNECTION_TYPE_INTERNAL),
    "PP_OUTPUT_PROTECTION_LINK_TYPE_PRIVATE_INTERNAL value mismatch");
static_assert(static_cast<int>(PP_OUTPUT_PROTECTION_LINK_TYPE_PRIVATE_VGA) ==
                  static_cast<int>(display::DISPLAY_CONNECTION_TYPE_VGA),
              "PP_OUTPUT_PROTECTION_LINK_TYPE_PRIVATE_VGA value mismatch");
static_assert(static_cast<int>(PP_OUTPUT_PROTECTION_LINK_TYPE_PRIVATE_HDMI) ==
                  static_cast<int>(display::DISPLAY_CONNECTION_TYPE_HDMI),
              "PP_OUTPUT_PROTECTION_LINK_TYPE_PRIVATE_HDMI value mismatch");
static_assert(static_cast<int>(PP_OUTPUT_PROTECTION_LINK_TYPE_PRIVATE_DVI) ==
                  static_cast<int>(display::DISPLAY_CONNECTION_TYPE_DVI),
              "PP_OUTPUT_PROTECTION_LINK_TYPE_PRIVATE_DVI value mismatch");
static_assert(
    static_cast<int>(PP_OUTPUT_PROTECTION_LINK_TYPE_PRIVATE_DISPLAYPORT) ==
        static_cast<int>(display::DISPLAY_CONNECTION_TYPE_DISPLAYPORT),
    "PP_OUTPUT_PROTECTION_LINK_TYPE_PRIVATE_DISPLAYPORT value mismatch");
static_assert(
    static_cast<int>(PP_OUTPUT_PROTECTION_LINK_TYPE_PRIVATE_NETWORK) ==
        static_cast<int>(display::DISPLAY_CONNECTION_TYPE_NETWORK),
    "PP_OUTPUT_PROTECTION_LINK_TYPE_PRIVATE_NETWORK value mismatch");
static_assert(static_cast<int>(PP_OUTPUT_PROTECTION_METHOD_PRIVATE_NONE) ==
                  static_cast<int>(display::CONTENT_PROTECTION_METHOD_NONE),
              "PP_OUTPUT_PROTECTION_METHOD_PRIVATE_NONE value mismatch");
static_assert(static_cast<int>(PP_OUTPUT_PROTECTION_METHOD_PRIVATE_HDCP) ==
                  static_cast<int>(display::CONTENT_PROTECTION_METHOD_HDCP),
              "PP_OUTPUT_PROTECTION_METHOD_PRIVATE_HDCP value mismatch");

}  // namespace

PepperOutputProtectionMessageFilter::PepperOutputProtectionMessageFilter(
    content::BrowserPpapiHost* host,
    PP_Instance instance)
    : weak_ptr_factory_(this) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  int render_process_id = 0;
  int render_frame_id = 0;
  host->GetRenderFrameIDsForInstance(
      instance, &render_process_id, &render_frame_id);
  proxy_.reset(new OutputProtectionProxy(render_process_id, render_frame_id));
}

PepperOutputProtectionMessageFilter::~PepperOutputProtectionMessageFilter() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
}

scoped_refptr<base::TaskRunner>
PepperOutputProtectionMessageFilter::OverrideTaskRunnerForMessage(
    const IPC::Message& message) {
  return content::BrowserThread::GetTaskRunnerForThread(
      content::BrowserThread::UI);
}

int32_t PepperOutputProtectionMessageFilter::OnResourceMessageReceived(
    const IPC::Message& msg,
    ppapi::host::HostMessageContext* context) {
  PPAPI_BEGIN_MESSAGE_MAP(PepperOutputProtectionMessageFilter, msg)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(
        PpapiHostMsg_OutputProtection_QueryStatus, OnQueryStatus);
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(
        PpapiHostMsg_OutputProtection_EnableProtection, OnEnableProtection);
  PPAPI_END_MESSAGE_MAP()
  return PP_ERROR_FAILED;
}

int32_t PepperOutputProtectionMessageFilter::OnQueryStatus(
    ppapi::host::HostMessageContext* context) {
  ppapi::host::ReplyMessageContext reply_context =
      context->MakeReplyMessageContext();
  proxy_->QueryStatus(
      base::Bind(&PepperOutputProtectionMessageFilter::OnQueryStatusComplete,
                 weak_ptr_factory_.GetWeakPtr(), reply_context));
  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperOutputProtectionMessageFilter::OnEnableProtection(
    ppapi::host::HostMessageContext* context,
    uint32_t desired_method_mask) {
  ppapi::host::ReplyMessageContext reply_context =
      context->MakeReplyMessageContext();
  proxy_->EnableProtection(
      desired_method_mask,
      base::Bind(
          &PepperOutputProtectionMessageFilter::OnEnableProtectionComplete,
          weak_ptr_factory_.GetWeakPtr(), reply_context));
  return PP_OK_COMPLETIONPENDING;
}

// static
void PepperOutputProtectionMessageFilter::OnQueryStatusComplete(
    base::WeakPtr<PepperOutputProtectionMessageFilter> filter,
    ppapi::host::ReplyMessageContext reply_context,
    bool success,
    uint32_t link_mask,
    uint32_t protection_mask) {
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(
          &PepperOutputProtectionMessageFilter::OnQueryStatusCompleteOnIOThread,
          filter, reply_context, success, link_mask, protection_mask));
}

void PepperOutputProtectionMessageFilter::OnQueryStatusCompleteOnIOThread(
    ppapi::host::ReplyMessageContext reply_context,
    bool success,
    uint32_t link_mask,
    uint32_t protection_mask) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  reply_context.params.set_result(success ? PP_OK : PP_ERROR_FAILED);
  SendReply(reply_context, PpapiPluginMsg_OutputProtection_QueryStatusReply(
                               link_mask, protection_mask));
}

// static
void PepperOutputProtectionMessageFilter::OnEnableProtectionComplete(
    base::WeakPtr<PepperOutputProtectionMessageFilter> filter,
    ppapi::host::ReplyMessageContext reply_context,
    bool success) {
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&PepperOutputProtectionMessageFilter::
                     OnEnableProtectionCompleteOnIOThread,
                 filter, reply_context, success));
}

void PepperOutputProtectionMessageFilter::OnEnableProtectionCompleteOnIOThread(
    ppapi::host::ReplyMessageContext reply_context,
    bool success) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  reply_context.params.set_result(success ? PP_OK : PP_ERROR_FAILED);
  SendReply(reply_context,
            PpapiPluginMsg_OutputProtection_EnableProtectionReply());
}
