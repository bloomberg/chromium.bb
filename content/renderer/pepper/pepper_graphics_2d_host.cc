// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/pepper_graphics_2d_host.h"

#include "content/public/renderer/renderer_ppapi_host.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/host/dispatch_host_message.h"
#include "ppapi/host/host_message_context.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/api_id.h"
#include "ppapi/thunk/enter.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"
#include "webkit/plugins/ppapi/ppb_graphics_2d_impl.h"  // TODO: merge to here

namespace content {

// static
PepperGraphics2DHost* PepperGraphics2DHost::Create(RendererPpapiHost* host,
                                                   PP_Instance instance,
                                                   PP_Resource resource,
                                                   const PP_Size& size,
                                                   PP_Bool is_always_opaque) {
  PepperGraphics2DHost* resource_host =
      new PepperGraphics2DHost(host, instance, resource);
  if (!resource_host->graphics_2d_->Init(size.width, size.height,
                                         PP_ToBool(is_always_opaque))) {
    delete resource_host;
    return NULL;
  }
  return resource_host;
}

PepperGraphics2DHost::PepperGraphics2DHost(RendererPpapiHost* host,
                                           PP_Instance instance,
                                           PP_Resource resource)
    : ResourceHost(host->GetPpapiHost(), instance, resource),
      graphics_2d_(new webkit::ppapi::PPB_Graphics2D_Impl(instance)),
      is_running_in_process_(host->IsRunningInProcess()) {
}

PepperGraphics2DHost::~PepperGraphics2DHost() {
  // Unbind from the instance when destoryed.
  PP_Instance instance = graphics_2d_->pp_instance();
  ppapi::thunk::EnterInstanceNoLock enter(instance);
  if (enter.succeeded())
    enter.functions()->BindGraphics(instance, 0);
}

int32_t PepperGraphics2DHost::OnResourceMessageReceived(
    const IPC::Message& msg,
    ppapi::host::HostMessageContext* context) {
  IPC_BEGIN_MESSAGE_MAP(PepperGraphics2DHost, msg)
#if !defined(OS_NACL)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(
        PpapiHostMsg_Graphics2D_PaintImageData,
        OnHostMsgPaintImageData)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(
        PpapiHostMsg_Graphics2D_Scroll,
        OnHostMsgScroll)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(
        PpapiHostMsg_Graphics2D_ReplaceContents,
        OnHostMsgReplaceContents)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(
        PpapiHostMsg_Graphics2D_Flush,
        OnHostMsgFlush)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(
        PpapiHostMsg_Graphics2D_Dev_SetScale,
        OnHostMsgSetScale)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(
        PpapiHostMsg_Graphics2D_ReadImageData,
        OnHostMsgReadImageData)
#endif
  IPC_END_MESSAGE_MAP()
  return PP_ERROR_FAILED;
}

bool PepperGraphics2DHost::ReadImageData(PP_Resource image,
                                         const PP_Point* top_left) {
  return graphics_2d_->ReadImageData(image, top_left);
}

bool PepperGraphics2DHost::BindToInstance(
    webkit::ppapi::PluginInstance* new_instance) {
  if (new_instance &&
      new_instance->pp_instance() != graphics_2d_->pp_instance())
    return false;  // Can't bind other instance's contexts.
  return graphics_2d_->BindToInstance(new_instance);
}

void PepperGraphics2DHost::Paint(WebKit::WebCanvas* canvas,
                                 const gfx::Rect& plugin_rect,
                                 const gfx::Rect& paint_rect) {
  graphics_2d_->Paint(canvas, plugin_rect, paint_rect);
}

void PepperGraphics2DHost::ViewWillInitiatePaint() {
  graphics_2d_->ViewWillInitiatePaint();
}

void PepperGraphics2DHost::ViewInitiatedPaint() {
  graphics_2d_->ViewInitiatedPaint();
}

void PepperGraphics2DHost::ViewFlushedPaint() {
  graphics_2d_->ViewFlushedPaint();
}

void PepperGraphics2DHost::SetScale(float scale) {
  graphics_2d_->scale_ = scale;
}

float PepperGraphics2DHost::GetScale() const {
  return graphics_2d_->scale_;
}

bool PepperGraphics2DHost::IsAlwaysOpaque() const {
  return graphics_2d_->is_always_opaque_;
}

webkit::ppapi::PPB_ImageData_Impl* PepperGraphics2DHost::ImageData() {
  return graphics_2d_->image_data_.get();
}

bool PepperGraphics2DHost::IsGraphics2DHost() const {
  return true;
}

#if !defined(OS_NACL)
int32_t PepperGraphics2DHost::OnHostMsgPaintImageData(
    ppapi::host::HostMessageContext* context,
    const ppapi::HostResource& image_data,
    const PP_Point& top_left,
    bool src_rect_specified,
    const PP_Rect& src_rect) {
  graphics_2d_->PaintImageData(image_data.host_resource(), &top_left,
                              src_rect_specified ? &src_rect : NULL);
  return PP_OK;
}

int32_t PepperGraphics2DHost::OnHostMsgScroll(
    ppapi::host::HostMessageContext* context,
    bool clip_specified,
    const PP_Rect& clip,
    const PP_Point& amount) {
  graphics_2d_->Scroll(clip_specified ? &clip : NULL, &amount);
  return PP_OK;
}

int32_t PepperGraphics2DHost::OnHostMsgReplaceContents(
    ppapi::host::HostMessageContext* context,
    const ppapi::HostResource& image_data) {
  graphics_2d_->ReplaceContents(image_data.host_resource());
  return PP_OK;
}

int32_t PepperGraphics2DHost::OnHostMsgFlush(
    ppapi::host::HostMessageContext* context) {
  PP_Resource old_image_data = 0;
  flush_reply_context_ = context->MakeReplyMessageContext();
  // TODO: when merge PP_Graphics2D_Impl, we won't need this tracked callback.
  scoped_refptr<ppapi::TrackedCallback> callback(new ppapi::TrackedCallback(
      graphics_2d_.get(),
      PP_MakeCompletionCallback(&PepperGraphics2DHost::SendFlushACKToPlugin,
                                AsWeakPtr())));
  if (is_running_in_process_)
    return graphics_2d_->Flush(callback, NULL);

  // Reuse image data when running out of process.
  int32_t result = graphics_2d_->Flush(callback, &old_image_data);

  if (old_image_data) {
    // If the Graphics2D has an old image data it's not using any more, send
    // it back to the plugin for possible re-use. See ppb_image_data_proxy.cc
    // for a description how this process works.
    ppapi::HostResource old_image_data_host_resource;
    old_image_data_host_resource.SetHostResource(pp_instance(),
                                                 old_image_data);
    host()->Send(new PpapiMsg_PPBImageData_NotifyUnusedImageData(
        ppapi::API_ID_PPB_IMAGE_DATA, old_image_data_host_resource));
  }

  return result;
}

int32_t PepperGraphics2DHost::OnHostMsgSetScale(
    ppapi::host::HostMessageContext* context,
    float scale) {
  return graphics_2d_->SetScale(scale) ? PP_OK : PP_ERROR_BADARGUMENT;
}

int32_t PepperGraphics2DHost::OnHostMsgReadImageData(
    ppapi::host::HostMessageContext* context,
    PP_Resource image,
    const PP_Point& top_left) {
  context->reply_msg = PpapiPluginMsg_Graphics2D_ReadImageDataAck();
  return ReadImageData(image, &top_left) ? PP_OK : PP_ERROR_FAILED;
}

// static
void PepperGraphics2DHost::SendFlushACKToPlugin(void* data,
                                                int32_t pp_error) {
  if (!data || pp_error != PP_OK)
    return;
  PepperGraphics2DHost* self = (PepperGraphics2DHost*) data;
  self->host()->SendReply(self->flush_reply_context_,
                          PpapiPluginMsg_Graphics2D_FlushAck());
}
#endif  // !defined(OS_NACL)

}  // namespace content
