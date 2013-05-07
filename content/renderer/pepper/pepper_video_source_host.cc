// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/pepper_video_source_host.h"

#include "base/bind.h"
#include "base/safe_numerics.h"
#include "content/public/renderer/renderer_ppapi_host.h"
#include "content/renderer/render_thread_impl.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/host/dispatch_host_message.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/ppb_image_data_proxy.h"
#include "ppapi/shared_impl/scoped_pp_resource.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_image_data_api.h"
#include "third_party/libjingle/source/talk/media/base/videocommon.h"
#include "third_party/libjingle/source/talk/media/base/videoframe.h"
#include "third_party/skia/include/core/SkBitmap.h"
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
      main_message_loop_proxy_(base::MessageLoopProxy::current()),
      source_handler_(new content::VideoSourceHandler(NULL)),
      get_frame_pending_(false) {
}

PepperVideoSourceHost::~PepperVideoSourceHost() {
  Close();
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

bool PepperVideoSourceHost::GotFrame(cricket::VideoFrame* frame) {
  // It's not safe to access this on another thread, so post a task to our
  // main thread to transfer the new frame.
  main_message_loop_proxy_->PostTask(
      FROM_HERE,
      base::Bind(&PepperVideoSourceHost::OnGotFrame,
                 weak_factory_.GetWeakPtr(),
                 base::Passed(scoped_ptr<cricket::VideoFrame>(frame))));

  return true;
}

void PepperVideoSourceHost::OnGotFrame(scoped_ptr<cricket::VideoFrame> frame) {
  // Take ownership of the new frame, and possibly delete any unsent one.
  last_frame_.swap(frame);

  if (get_frame_pending_) {
    ppapi::HostResource image_data_resource;
    PP_TimeTicks timestamp = 0;
    int32_t result = ConvertFrame(&image_data_resource, &timestamp);
    SendFrame(image_data_resource, timestamp, result);
  }
}

int32_t PepperVideoSourceHost::OnHostMsgOpen(HostMessageContext* context,
                                             const std::string& stream_url) {
  GURL gurl(stream_url);
  if (!gurl.is_valid())
    return PP_ERROR_BADARGUMENT;

  if (!source_handler_->Open(gurl.spec(), this))
    return PP_ERROR_BADARGUMENT;

  stream_url_ = gurl.spec();

  ReplyMessageContext reply_context = context->MakeReplyMessageContext();
  reply_context.params.set_result(PP_OK);
  host()->SendReply(reply_context, PpapiPluginMsg_VideoSource_OpenReply());
  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperVideoSourceHost::OnHostMsgGetFrame(
    HostMessageContext* context) {
  if (!source_handler_.get())
    return PP_ERROR_FAILED;
  if (get_frame_pending_)
    return PP_ERROR_INPROGRESS;

  reply_context_ = context->MakeReplyMessageContext();
  get_frame_pending_ = true;

  // If a frame is ready, try to convert it and reply.
  if (last_frame_.get()) {
    ppapi::HostResource image_data_resource;
    PP_TimeTicks timestamp = 0;
    int32_t result = ConvertFrame(&image_data_resource, &timestamp);
    if (result == PP_OK) {
      SendFrame(image_data_resource, timestamp, result);
    } else {
      reply_context_ = ppapi::host::ReplyMessageContext();
      return result;
    }
  }

  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperVideoSourceHost::OnHostMsgClose(HostMessageContext* context) {
  Close();
  return PP_OK;
}

int32_t PepperVideoSourceHost::ConvertFrame(
    ppapi::HostResource* image_data_resource,
    PP_TimeTicks* timestamp) {
  DCHECK(last_frame_.get());
  scoped_ptr<cricket::VideoFrame> frame(last_frame_.release());

  int32_t width = base::checked_numeric_cast<int32_t>(frame->GetWidth());
  int32_t height = base::checked_numeric_cast<int32_t>(frame->GetHeight());
  // Create an image data resource to hold the frame pixels.
  PP_ImageDataDesc desc;
  IPC::PlatformFileForTransit image_handle;
  uint32_t byte_count;
  ppapi::ScopedPPResource resource(
      ppapi::ScopedPPResource::PassRef(),
      ppapi::proxy::PPB_ImageData_Proxy::CreateImageData(
          pp_instance(),
          PP_IMAGEDATAFORMAT_BGRA_PREMUL,
          PP_MakeSize(width, height),
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

  const SkBitmap* bitmap = image_data->GetMappedBitmap();
  if (!bitmap)
    return PP_ERROR_FAILED;
  uint8_t* bitmap_pixels = static_cast<uint8_t*>(bitmap->getPixels());
  if (!bitmap_pixels)
    return PP_ERROR_FAILED;

  size_t bitmap_size = bitmap->getSize();
  frame->ConvertToRgbBuffer(cricket::FOURCC_BGRA,
                            bitmap_pixels,
                            bitmap_size,
                            bitmap->rowBytes());

  image_data_resource->SetHostResource(pp_instance(), resource.get());

  // Convert a video timestamp (int64, in nanoseconds) to a time delta (int64,
  // microseconds) and then to a PP_TimeTicks (a double, in seconds). All times
  // are relative to the Unix Epoch.
  base::TimeDelta time_delta = base::TimeDelta::FromMicroseconds(
      frame->GetTimeStamp() / base::Time::kNanosecondsPerMicrosecond);
  *timestamp = time_delta.InSecondsF();
  return PP_OK;
}

void PepperVideoSourceHost::SendFrame(
    const ppapi::HostResource& image_data_resource,
    PP_TimeTicks timestamp,
    int32_t result) {
  DCHECK(get_frame_pending_);
  reply_context_.params.set_result(result);
  host()->SendReply(
      reply_context_,
      PpapiPluginMsg_VideoSource_GetFrameReply(image_data_resource, timestamp));

  reply_context_ = ppapi::host::ReplyMessageContext();
  get_frame_pending_ = false;
}

void PepperVideoSourceHost::Close() {
  if (source_handler_.get()) {
    source_handler_->Close(stream_url_, this);
    source_handler_.reset(NULL);
  }
}

}  // namespace content
