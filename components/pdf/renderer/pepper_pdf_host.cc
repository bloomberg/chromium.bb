// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/pdf/renderer/pepper_pdf_host.h"

#include "components/pdf/common/pdf_messages.h"
#include "components/pdf/renderer/pdf_resource_util.h"
#include "components/pdf/renderer/ppb_pdf_impl.h"
#include "content/public/common/referrer.h"
#include "content/public/renderer/pepper_plugin_instance.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "content/public/renderer/renderer_ppapi_host.h"
#include "ppapi/host/dispatch_host_message.h"
#include "ppapi/host/host_message_context.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/proxy/host_dispatcher.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/ppb_image_data_proxy.h"
#include "ppapi/shared_impl/ppb_image_data_shared.h"
#include "ppapi/shared_impl/scoped_pp_resource.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_image_data_api.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebPluginContainer.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/layout.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/gfx/point.h"

namespace pdf {

PepperPDFHost::PepperPDFHost(content::RendererPpapiHost* host,
                             PP_Instance instance,
                             PP_Resource resource)
    : ppapi::host::ResourceHost(host->GetPpapiHost(), instance, resource),
      host_(host) {}

PepperPDFHost::~PepperPDFHost() {}

int32_t PepperPDFHost::OnResourceMessageReceived(
    const IPC::Message& msg,
    ppapi::host::HostMessageContext* context) {
  PPAPI_BEGIN_MESSAGE_MAP(PepperPDFHost, msg)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_PDF_GetLocalizedString,
                                      OnHostMsgGetLocalizedString)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(PpapiHostMsg_PDF_DidStartLoading,
                                        OnHostMsgDidStartLoading)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(PpapiHostMsg_PDF_DidStopLoading,
                                        OnHostMsgDidStopLoading)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_PDF_UserMetricsRecordAction,
                                      OnHostMsgUserMetricsRecordAction)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(PpapiHostMsg_PDF_HasUnsupportedFeature,
                                        OnHostMsgHasUnsupportedFeature)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(PpapiHostMsg_PDF_Print, OnHostMsgPrint)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(PpapiHostMsg_PDF_SaveAs,
                                        OnHostMsgSaveAs)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_PDF_GetResourceImage,
                                      OnHostMsgGetResourceImage)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_PDF_SetSelectedText,
                                      OnHostMsgSetSelectedText)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_PDF_SetLinkUnderCursor,
                                      OnHostMsgSetLinkUnderCursor)
  PPAPI_END_MESSAGE_MAP()
  return PP_ERROR_FAILED;
}

int32_t PepperPDFHost::OnHostMsgGetLocalizedString(
    ppapi::host::HostMessageContext* context,
    PP_ResourceString string_id) {
  std::string rv = GetStringResource(string_id);
  context->reply_msg = PpapiPluginMsg_PDF_GetLocalizedStringReply(rv);
  return PP_OK;
}

int32_t PepperPDFHost::OnHostMsgDidStartLoading(
    ppapi::host::HostMessageContext* context) {
  content::PepperPluginInstance* instance =
      host_->GetPluginInstance(pp_instance());
  if (!instance)
    return PP_ERROR_FAILED;
  instance->GetRenderView()->DidStartLoading();
  return PP_OK;
}

int32_t PepperPDFHost::OnHostMsgDidStopLoading(
    ppapi::host::HostMessageContext* context) {
  content::PepperPluginInstance* instance =
      host_->GetPluginInstance(pp_instance());
  if (!instance)
    return PP_ERROR_FAILED;
  instance->GetRenderView()->DidStopLoading();
  return PP_OK;
}

int32_t PepperPDFHost::OnHostMsgSetContentRestriction(
    ppapi::host::HostMessageContext* context,
    int restrictions) {
  content::PepperPluginInstance* instance =
      host_->GetPluginInstance(pp_instance());
  if (!instance)
    return PP_ERROR_FAILED;
  instance->GetRenderView()->Send(new PDFHostMsg_PDFUpdateContentRestrictions(
      instance->GetRenderView()->GetRoutingID(), restrictions));
  return PP_OK;
}

int32_t PepperPDFHost::OnHostMsgUserMetricsRecordAction(
    ppapi::host::HostMessageContext* context,
    const std::string& action) {
  if (action.empty())
    return PP_ERROR_FAILED;
  content::RenderThread::Get()->RecordComputedAction(action);
  return PP_OK;
}

int32_t PepperPDFHost::OnHostMsgHasUnsupportedFeature(
    ppapi::host::HostMessageContext* context) {
  content::PepperPluginInstance* instance =
      host_->GetPluginInstance(pp_instance());
  if (!instance)
    return PP_ERROR_FAILED;

  blink::WebView* view =
      instance->GetContainer()->element().document().frame()->view();
  content::RenderView* render_view = content::RenderView::FromWebView(view);
  render_view->Send(
      new PDFHostMsg_PDFHasUnsupportedFeature(render_view->GetRoutingID()));
  return PP_OK;
}

int32_t PepperPDFHost::OnHostMsgPrint(
    ppapi::host::HostMessageContext* context) {
  return PPB_PDF_Impl::InvokePrintingForInstance(pp_instance()) ? PP_OK :
      PP_ERROR_FAILED;
}

int32_t PepperPDFHost::OnHostMsgSaveAs(
    ppapi::host::HostMessageContext* context) {
  content::PepperPluginInstance* instance =
      host_->GetPluginInstance(pp_instance());
  if (!instance)
    return PP_ERROR_FAILED;
  GURL url = instance->GetPluginURL();
  content::RenderView* render_view = instance->GetRenderView();
  blink::WebLocalFrame* frame =
      render_view->GetWebView()->mainFrame()->toWebLocalFrame();
  content::Referrer referrer(frame->document().url(),
                             frame->document().referrerPolicy());
  render_view->Send(
      new PDFHostMsg_PDFSaveURLAs(render_view->GetRoutingID(), url, referrer));
  return PP_OK;
}

int32_t PepperPDFHost::OnHostMsgGetResourceImage(
    ppapi::host::HostMessageContext* context,
    PP_ResourceImage image_id,
    float scale) {
  gfx::ImageSkia* res_image_skia = GetImageResource(image_id);

  if (!res_image_skia)
    return PP_ERROR_FAILED;

  gfx::ImageSkiaRep image_skia_rep = res_image_skia->GetRepresentation(scale);

  if (image_skia_rep.is_null() || image_skia_rep.scale() != scale)
    return PP_ERROR_FAILED;

  PP_Size pp_size;
  pp_size.width = image_skia_rep.pixel_width();
  pp_size.height = image_skia_rep.pixel_height();

  ppapi::HostResource host_resource;
  PP_ImageDataDesc image_data_desc;
  IPC::PlatformFileForTransit image_handle;
  uint32_t byte_count = 0;
  bool success =
      CreateImageData(pp_instance(),
                      ppapi::PPB_ImageData_Shared::GetNativeImageDataFormat(),
                      pp_size,
                      image_skia_rep.sk_bitmap(),
                      &host_resource,
                      &image_data_desc,
                      &image_handle,
                      &byte_count);
  ppapi::ScopedPPResource image_data_resource(
      ppapi::ScopedPPResource::PassRef(), host_resource.host_resource());
  if (!success)
    return PP_ERROR_FAILED;

  ppapi::host::ReplyMessageContext reply_context =
      context->MakeReplyMessageContext();
  ppapi::proxy::SerializedHandle serialized_handle;
  serialized_handle.set_shmem(image_handle, byte_count);
  reply_context.params.AppendHandle(serialized_handle);
  SendReply(
      reply_context,
      PpapiPluginMsg_PDF_GetResourceImageReply(host_resource, image_data_desc));

  // Keep a reference to the resource only if the function succeeds.
  image_data_resource.Release();

  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperPDFHost::OnHostMsgSetSelectedText(
    ppapi::host::HostMessageContext* context,
    const base::string16& selected_text) {
  content::PepperPluginInstance* instance =
      host_->GetPluginInstance(pp_instance());
  if (!instance)
    return PP_ERROR_FAILED;
  instance->SetSelectedText(selected_text);
  return PP_OK;
}

int32_t PepperPDFHost::OnHostMsgSetLinkUnderCursor(
    ppapi::host::HostMessageContext* context,
    const std::string& url) {
  content::PepperPluginInstance* instance =
      host_->GetPluginInstance(pp_instance());
  if (!instance)
    return PP_ERROR_FAILED;
  instance->SetLinkUnderCursor(url);
  return PP_OK;
}

// TODO(raymes): This function is mainly copied from ppb_image_data_proxy.cc.
// It's a mess and needs to be fixed in several ways but this is better done
// when we refactor PPB_ImageData. On success, the image handle will be
// non-null.
bool PepperPDFHost::CreateImageData(
    PP_Instance instance,
    PP_ImageDataFormat format,
    const PP_Size& size,
    const SkBitmap& pixels_to_write,
    ppapi::HostResource* result,
    PP_ImageDataDesc* out_image_data_desc,
    IPC::PlatformFileForTransit* out_image_handle,
    uint32_t* out_byte_count) {
  PP_Resource resource = ppapi::proxy::PPB_ImageData_Proxy::CreateImageData(
      instance,
      ppapi::PPB_ImageData_Shared::SIMPLE,
      format,
      size,
      false /* init_to_zero */,
      out_image_data_desc,
      out_image_handle,
      out_byte_count);
  if (!resource)
    return false;

  result->SetHostResource(instance, resource);

  // Write the image to the resource shared memory.
  ppapi::thunk::EnterResourceNoLock<ppapi::thunk::PPB_ImageData_API>
      enter_resource(resource, false);
  if (enter_resource.failed())
    return false;

  ppapi::thunk::PPB_ImageData_API* image_data =
      static_cast<ppapi::thunk::PPB_ImageData_API*>(enter_resource.object());
  SkCanvas* canvas = image_data->GetCanvas();
  bool needs_unmapping = false;
  if (!canvas) {
    needs_unmapping = true;
    image_data->Map();
    canvas = image_data->GetCanvas();
    if (!canvas)
      return false;  // Failure mapping.
  }

  const SkBitmap* bitmap = &skia::GetTopDevice(*canvas)->accessBitmap(false);
  pixels_to_write.copyPixelsTo(
      bitmap->getPixels(), bitmap->getSize(), bitmap->rowBytes());

  if (needs_unmapping)
    image_data->Unmap();

  return true;
}

}  // namespace pdf
