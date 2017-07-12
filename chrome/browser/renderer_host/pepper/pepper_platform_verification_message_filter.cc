// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/pepper/pepper_platform_verification_message_filter.h"

#include "base/bind_helpers.h"
#include "content/public/browser/browser_ppapi_host.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/host/dispatch_host_message.h"
#include "ppapi/host/host_message_context.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/proxy/ppapi_messages.h"

namespace chrome {

PepperPlatformVerificationMessageFilter::
    PepperPlatformVerificationMessageFilter(content::BrowserPpapiHost* host,
                                            PP_Instance instance)
    : render_process_id_(0), render_frame_id_(0) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  host->GetRenderFrameIDsForInstance(
      instance, &render_process_id_, &render_frame_id_);
}

PepperPlatformVerificationMessageFilter::
    ~PepperPlatformVerificationMessageFilter() {}

scoped_refptr<base::TaskRunner>
PepperPlatformVerificationMessageFilter::OverrideTaskRunnerForMessage(
    const IPC::Message& msg) {
  return content::BrowserThread::GetTaskRunnerForThread(
      content::BrowserThread::UI);
}

int32_t PepperPlatformVerificationMessageFilter::OnResourceMessageReceived(
    const IPC::Message& msg,
    ppapi::host::HostMessageContext* context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  PPAPI_BEGIN_MESSAGE_MAP(PepperPlatformVerificationMessageFilter, msg)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(
        PpapiHostMsg_PlatformVerification_ChallengePlatform,
        OnChallengePlatform)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(
        PpapiHostMsg_PlatformVerification_GetStorageId, OnGetStorageId)
  PPAPI_END_MESSAGE_MAP()

  return PP_ERROR_FAILED;
}

int32_t PepperPlatformVerificationMessageFilter::OnChallengePlatform(
    ppapi::host::HostMessageContext* context,
    const std::string& service_id,
    const std::vector<uint8_t>& challenge) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

#if defined(OS_CHROMEOS)
  content::RenderFrameHost* rfh =
      content::RenderFrameHost::FromID(render_process_id_, render_frame_id_);
  if (rfh) {
    if (!pv_)
      pv_ = new chromeos::attestation::PlatformVerificationFlow();

    pv_->ChallengePlatformKey(
        content::WebContents::FromRenderFrameHost(rfh), service_id,
        std::string(challenge.begin(), challenge.end()),
        base::Bind(
            &PepperPlatformVerificationMessageFilter::ChallengePlatformCallback,
            this, context->MakeReplyMessageContext()));
    return PP_OK_COMPLETIONPENDING;
  }
#else
  NOTREACHED() << "Challenging platform is only supported on ChromeOS.";
#endif

  ppapi::host::ReplyMessageContext reply_context =
      context->MakeReplyMessageContext();
  reply_context.params.set_result(PP_ERROR_FAILED);
  SendReply(reply_context,
            PpapiHostMsg_PlatformVerification_ChallengePlatformReply(
                std::vector<uint8_t>(), std::vector<uint8_t>(), std::string()));
  return PP_OK_COMPLETIONPENDING;
}

#if defined(OS_CHROMEOS)
void PepperPlatformVerificationMessageFilter::ChallengePlatformCallback(
    ppapi::host::ReplyMessageContext reply_context,
    chromeos::attestation::PlatformVerificationFlow::Result challenge_result,
    const std::string& signed_data,
    const std::string& signature,
    const std::string& platform_key_certificate) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (challenge_result ==
      chromeos::attestation::PlatformVerificationFlow::SUCCESS) {
    reply_context.params.set_result(PP_OK);
  } else {
    reply_context.params.set_result(PP_ERROR_FAILED);
    DCHECK_EQ(signed_data.size(), 0u);
    DCHECK_EQ(signature.size(), 0u);
    DCHECK_EQ(platform_key_certificate.size(), 0u);
  }

  SendReply(reply_context,
            PpapiHostMsg_PlatformVerification_ChallengePlatformReply(
                std::vector<uint8_t>(signed_data.begin(), signed_data.end()),
                std::vector<uint8_t>(signature.begin(), signature.end()),
                platform_key_certificate));
}
#endif

int32_t PepperPlatformVerificationMessageFilter::OnGetStorageId(
    ppapi::host::HostMessageContext* context) {
  // TODO(jrummell): Implement Storage ID. For now simply returns empty string.
  // http://crbug.com/478960.
  ppapi::host::ReplyMessageContext reply_context =
      context->MakeReplyMessageContext();
  reply_context.params.set_result(PP_OK);
  SendReply(reply_context,
            PpapiHostMsg_PlatformVerification_GetStorageIdReply(std::string()));
  return PP_OK_COMPLETIONPENDING;
}

}  // namespace chrome
