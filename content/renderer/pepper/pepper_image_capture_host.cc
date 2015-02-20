// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/pepper_image_capture_host.h"

#include "content/renderer/pepper/pepper_platform_image_capture.h"
#include "content/renderer/pepper/renderer_ppapi_host_impl.h"
#include "content/renderer/render_frame_impl.h"
#include "ppapi/host/dispatch_host_message.h"
#include "ppapi/proxy/ppapi_messages.h"

namespace content {

PepperImageCaptureHost::PepperImageCaptureHost(RendererPpapiHostImpl* host,
                                               PP_Instance instance,
                                               PP_Resource resource)
    : ResourceHost(host->GetPpapiHost(), instance, resource),
      renderer_ppapi_host_(host) {
}

PepperImageCaptureHost::~PepperImageCaptureHost() {
  DetachPlatformImageCapture();
}

bool PepperImageCaptureHost::Init() {
  return !!renderer_ppapi_host_->GetPluginInstance(pp_instance());
}

int32_t PepperImageCaptureHost::OnResourceMessageReceived(
    const IPC::Message& msg,
    ppapi::host::HostMessageContext* context) {
  int32_t result = PP_ERROR_FAILED;

  PPAPI_BEGIN_MESSAGE_MAP(PepperImageCaptureHost, msg)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_ImageCapture_Open, OnOpen)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(
        PpapiHostMsg_ImageCapture_GetSupportedVideoCaptureFormats,
        OnGetSupportedVideoCaptureFormats)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(PpapiHostMsg_ImageCapture_Close,
                                        OnClose)
  PPAPI_END_MESSAGE_MAP()
  return result;
}

void PepperImageCaptureHost::OnInitialized(bool succeeded) {
  if (!open_reply_context_.is_valid())
    return;

  if (succeeded) {
    open_reply_context_.params.set_result(PP_OK);
  } else {
    DetachPlatformImageCapture();
    open_reply_context_.params.set_result(PP_ERROR_FAILED);
  }

  host()->SendReply(open_reply_context_,
                    PpapiPluginMsg_ImageCapture_OpenReply());
  open_reply_context_ = ppapi::host::ReplyMessageContext();
}

void PepperImageCaptureHost::OnVideoCaptureFormatsEnumerated(
    const std::vector<PP_VideoCaptureFormat>& formats) {
  if (!video_capture_formats_reply_context_.is_valid())
    return;

  if (formats.size() > 0)
    video_capture_formats_reply_context_.params.set_result(PP_OK);
  else
    video_capture_formats_reply_context_.params.set_result(PP_ERROR_FAILED);
  host()->SendReply(
      video_capture_formats_reply_context_,
      PpapiPluginMsg_ImageCapture_GetSupportedVideoCaptureFormatsReply(
          formats));
  video_capture_formats_reply_context_ = ppapi::host::ReplyMessageContext();
}

int32_t PepperImageCaptureHost::OnOpen(ppapi::host::HostMessageContext* context,
                                       const std::string& device_id) {
  if (open_reply_context_.is_valid())
    return PP_ERROR_INPROGRESS;

  if (platform_image_capture_.get())
    return PP_ERROR_FAILED;

  GURL document_url = renderer_ppapi_host_->GetDocumentURL(pp_instance());
  if (!document_url.is_valid())
    return PP_ERROR_FAILED;

  platform_image_capture_.reset(new PepperPlatformImageCapture(
      renderer_ppapi_host_->GetRenderFrameForInstance(pp_instance())
          ->GetRoutingID(),
      device_id, document_url, this));

  open_reply_context_ = context->MakeReplyMessageContext();

  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperImageCaptureHost::OnClose(
    ppapi::host::HostMessageContext* context) {
  DetachPlatformImageCapture();
  return PP_OK;
}

int32_t PepperImageCaptureHost::OnGetSupportedVideoCaptureFormats(
    ppapi::host::HostMessageContext* context) {
  if (video_capture_formats_reply_context_.is_valid())
    return PP_ERROR_INPROGRESS;
  if (!platform_image_capture_)
    return PP_ERROR_FAILED;

  video_capture_formats_reply_context_ = context->MakeReplyMessageContext();
  platform_image_capture_->GetSupportedVideoCaptureFormats();

  return PP_OK_COMPLETIONPENDING;
}

void PepperImageCaptureHost::DetachPlatformImageCapture() {
  if (platform_image_capture_) {
    platform_image_capture_->DetachEventHandler();
    platform_image_capture_.reset();
  }
}

}  // namespace content
