// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/pepper_video_source_host.h"

#include "base/bind.h"
#include "content/public/renderer/renderer_ppapi_host.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/host/dispatch_host_message.h"
#include "ppapi/host/host_message_context.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/ppb_image_data_proxy.h"
#include "ppapi/shared_impl/scoped_pp_resource.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_image_data_api.h"
#include "skia/ext/platform_canvas.h"
#include "webkit/plugins/ppapi/ppb_image_data_impl.h"

using ppapi::host::HostMessageContext;
using ppapi::host::ReplyMessageContext;

namespace content {

PepperVideoSourceHost::PepperVideoSourceHost(
    RendererPpapiHost* host,
    PP_Instance instance,
    PP_Resource resource)
    : ResourceHost(host->GetPpapiHost(), instance, resource),
      renderer_ppapi_host_(host),
      weak_factory_(this),
      last_timestamp_(0) {
}

PepperVideoSourceHost::~PepperVideoSourceHost() {
}

int32_t PepperVideoSourceHost::OnResourceMessageReceived(
    const IPC::Message& msg,
    HostMessageContext* context) {
  IPC_BEGIN_MESSAGE_MAP(PepperVideoSourceHost, msg)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_VideoSource_Open,
                                      OnHostMsgOpen)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(PpapiHostMsg_VideoSource_GetFrame,
                                        OnHostMsgGetFrame)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(PpapiHostMsg_VideoSource_Close,
                                        OnHostMsgClose)
  IPC_END_MESSAGE_MAP()
  return PP_ERROR_FAILED;
}

int32_t PepperVideoSourceHost::OnHostMsgOpen(HostMessageContext* context,
                                             const std::string& stream_url) {
  GURL gurl(stream_url);
  if (!gurl.is_valid())
    return PP_ERROR_BADARGUMENT;
  // TODO(ronghuawu) Check that gurl is a valid MediaStream video track URL.
  // TODO(ronghuawu) Open a MediaStream video track.
  ReplyMessageContext reply_context = context->MakeReplyMessageContext();
  reply_context.params.set_result(PP_OK);
  host()->SendReply(reply_context, PpapiPluginMsg_VideoSource_OpenReply());
  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperVideoSourceHost::OnHostMsgGetFrame(
    HostMessageContext* context) {
  ReplyMessageContext reply_context = context->MakeReplyMessageContext();
  // TODO(ronghuawu) Wait until a frame with timestamp > last_timestamp_ is
  // available.
  // Create an image data resource to hold the frame pixels.
  PP_ImageDataDesc desc;
  IPC::PlatformFileForTransit image_handle;
  uint32_t byte_count;
  ppapi::ScopedPPResource resource(
      ppapi::ScopedPPResource::PassRef(),
      ppapi::proxy::PPB_ImageData_Proxy::CreateImageData(
          pp_instance(),
          webkit::ppapi::PPB_ImageData_Impl::GetNativeImageDataFormat(),
          PP_MakeSize(0, 0),
          false /* init_to_zero */,
          false /* is_nacl_plugin */,
          &desc, &image_handle, &byte_count));
  if (!resource.get())
    return PP_ERROR_FAILED;

  ppapi::thunk::EnterResourceNoLock<ppapi::thunk::PPB_ImageData_API>
      enter_resource(resource, false);
  if (enter_resource.failed())
    return PP_ERROR_FAILED;

  webkit::ppapi::PPB_ImageData_Impl* image_data =
      static_cast<webkit::ppapi::PPB_ImageData_Impl*>(enter_resource.object());
  webkit::ppapi::ImageDataAutoMapper mapper(image_data);
  if (!mapper.is_valid())
    return PP_ERROR_FAILED;

  // TODO(ronghuawu) Copy frame pixels to canvas.

  ppapi::HostResource image_data_resource;
  image_data_resource.SetHostResource(pp_instance(), resource.get());
  double timestamp = 0;
  reply_context.params.set_result(PP_OK);
  host()->SendReply(
      reply_context,
      PpapiPluginMsg_VideoSource_GetFrameReply(image_data_resource, timestamp));
  last_timestamp_ = timestamp;
  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperVideoSourceHost::OnHostMsgClose(HostMessageContext* context) {
  // TODO(ronghuawu) Close the video stream.
  return PP_OK;
}

}  // namespace content
