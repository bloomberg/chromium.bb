// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/pepper/pepper_pdf_host.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/common/render_messages.h"
#include "chrome/renderer/printing/print_web_view_helper.h"
#include "content/app/resources/grit/content_resources.h"
#include "content/app/strings/grit/content_strings.h"
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
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/layout.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/gfx/point.h"

namespace {

struct ResourceImageInfo {
  PP_ResourceImage pp_id;
  int res_id;
};

const ResourceImageInfo kResourceImageMap[] = {
    {PP_RESOURCEIMAGE_PDF_BUTTON_FTP, IDR_PDF_BUTTON_FTP},
    {PP_RESOURCEIMAGE_PDF_BUTTON_FTP_HOVER, IDR_PDF_BUTTON_FTP_HOVER},
    {PP_RESOURCEIMAGE_PDF_BUTTON_FTP_PRESSED, IDR_PDF_BUTTON_FTP_PRESSED},
    {PP_RESOURCEIMAGE_PDF_BUTTON_FTW, IDR_PDF_BUTTON_FTW},
    {PP_RESOURCEIMAGE_PDF_BUTTON_FTW_HOVER, IDR_PDF_BUTTON_FTW_HOVER},
    {PP_RESOURCEIMAGE_PDF_BUTTON_FTW_PRESSED, IDR_PDF_BUTTON_FTW_PRESSED},
    {PP_RESOURCEIMAGE_PDF_BUTTON_ZOOMIN_END, IDR_PDF_BUTTON_ZOOMIN_END},
    {PP_RESOURCEIMAGE_PDF_BUTTON_ZOOMIN_END_HOVER,
     IDR_PDF_BUTTON_ZOOMIN_END_HOVER},
    {PP_RESOURCEIMAGE_PDF_BUTTON_ZOOMIN_END_PRESSED,
     IDR_PDF_BUTTON_ZOOMIN_END_PRESSED},
    {PP_RESOURCEIMAGE_PDF_BUTTON_ZOOMIN, IDR_PDF_BUTTON_ZOOMIN},
    {PP_RESOURCEIMAGE_PDF_BUTTON_ZOOMIN_HOVER, IDR_PDF_BUTTON_ZOOMIN_HOVER},
    {PP_RESOURCEIMAGE_PDF_BUTTON_ZOOMIN_PRESSED, IDR_PDF_BUTTON_ZOOMIN_PRESSED},
    {PP_RESOURCEIMAGE_PDF_BUTTON_ZOOMOUT, IDR_PDF_BUTTON_ZOOMOUT},
    {PP_RESOURCEIMAGE_PDF_BUTTON_ZOOMOUT_HOVER, IDR_PDF_BUTTON_ZOOMOUT_HOVER},
    {PP_RESOURCEIMAGE_PDF_BUTTON_ZOOMOUT_PRESSED,
     IDR_PDF_BUTTON_ZOOMOUT_PRESSED},
    {PP_RESOURCEIMAGE_PDF_BUTTON_SAVE, IDR_PDF_BUTTON_SAVE},
    {PP_RESOURCEIMAGE_PDF_BUTTON_SAVE_HOVER, IDR_PDF_BUTTON_SAVE_HOVER},
    {PP_RESOURCEIMAGE_PDF_BUTTON_SAVE_PRESSED, IDR_PDF_BUTTON_SAVE_PRESSED},
    {PP_RESOURCEIMAGE_PDF_BUTTON_PRINT, IDR_PDF_BUTTON_PRINT},
    {PP_RESOURCEIMAGE_PDF_BUTTON_PRINT_HOVER, IDR_PDF_BUTTON_PRINT_HOVER},
    {PP_RESOURCEIMAGE_PDF_BUTTON_PRINT_PRESSED, IDR_PDF_BUTTON_PRINT_PRESSED},
    {PP_RESOURCEIMAGE_PDF_BUTTON_PRINT_DISABLED, IDR_PDF_BUTTON_PRINT_DISABLED},
    {PP_RESOURCEIMAGE_PDF_BUTTON_THUMBNAIL_0, IDR_PDF_THUMBNAIL_0},
    {PP_RESOURCEIMAGE_PDF_BUTTON_THUMBNAIL_1, IDR_PDF_THUMBNAIL_1},
    {PP_RESOURCEIMAGE_PDF_BUTTON_THUMBNAIL_2, IDR_PDF_THUMBNAIL_2},
    {PP_RESOURCEIMAGE_PDF_BUTTON_THUMBNAIL_3, IDR_PDF_THUMBNAIL_3},
    {PP_RESOURCEIMAGE_PDF_BUTTON_THUMBNAIL_4, IDR_PDF_THUMBNAIL_4},
    {PP_RESOURCEIMAGE_PDF_BUTTON_THUMBNAIL_5, IDR_PDF_THUMBNAIL_5},
    {PP_RESOURCEIMAGE_PDF_BUTTON_THUMBNAIL_6, IDR_PDF_THUMBNAIL_6},
    {PP_RESOURCEIMAGE_PDF_BUTTON_THUMBNAIL_7, IDR_PDF_THUMBNAIL_7},
    {PP_RESOURCEIMAGE_PDF_BUTTON_THUMBNAIL_8, IDR_PDF_THUMBNAIL_8},
    {PP_RESOURCEIMAGE_PDF_BUTTON_THUMBNAIL_9, IDR_PDF_THUMBNAIL_9},
    {PP_RESOURCEIMAGE_PDF_BUTTON_THUMBNAIL_NUM_BACKGROUND,
     IDR_PDF_THUMBNAIL_NUM_BACKGROUND},
    {PP_RESOURCEIMAGE_PDF_PROGRESS_BAR_0, IDR_PDF_PROGRESS_BAR_0},
    {PP_RESOURCEIMAGE_PDF_PROGRESS_BAR_1, IDR_PDF_PROGRESS_BAR_1},
    {PP_RESOURCEIMAGE_PDF_PROGRESS_BAR_2, IDR_PDF_PROGRESS_BAR_2},
    {PP_RESOURCEIMAGE_PDF_PROGRESS_BAR_3, IDR_PDF_PROGRESS_BAR_3},
    {PP_RESOURCEIMAGE_PDF_PROGRESS_BAR_4, IDR_PDF_PROGRESS_BAR_4},
    {PP_RESOURCEIMAGE_PDF_PROGRESS_BAR_5, IDR_PDF_PROGRESS_BAR_5},
    {PP_RESOURCEIMAGE_PDF_PROGRESS_BAR_6, IDR_PDF_PROGRESS_BAR_6},
    {PP_RESOURCEIMAGE_PDF_PROGRESS_BAR_7, IDR_PDF_PROGRESS_BAR_7},
    {PP_RESOURCEIMAGE_PDF_PROGRESS_BAR_8, IDR_PDF_PROGRESS_BAR_8},
    {PP_RESOURCEIMAGE_PDF_PROGRESS_BAR_BACKGROUND,
     IDR_PDF_PROGRESS_BAR_BACKGROUND},
    {PP_RESOURCEIMAGE_PDF_PAGE_INDICATOR_BACKGROUND,
     IDR_PDF_PAGE_INDICATOR_BACKGROUND},
    {PP_RESOURCEIMAGE_PDF_PAGE_DROPSHADOW, IDR_PDF_PAGE_DROPSHADOW},
    {PP_RESOURCEIMAGE_PDF_PAN_SCROLL_ICON, IDR_PAN_SCROLL_ICON}, };

}  // namespace

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
  std::string rv;
  if (string_id == PP_RESOURCESTRING_PDFGETPASSWORD) {
    rv = base::UTF16ToUTF8(l10n_util::GetStringUTF16(IDS_PDF_NEED_PASSWORD));
  } else if (string_id == PP_RESOURCESTRING_PDFLOADING) {
    rv = base::UTF16ToUTF8(l10n_util::GetStringUTF16(IDS_PDF_PAGE_LOADING));
  } else if (string_id == PP_RESOURCESTRING_PDFLOAD_FAILED) {
    rv = base::UTF16ToUTF8(l10n_util::GetStringUTF16(IDS_PDF_PAGE_LOAD_FAILED));
  } else if (string_id == PP_RESOURCESTRING_PDFPROGRESSLOADING) {
    rv = base::UTF16ToUTF8(l10n_util::GetStringUTF16(IDS_PDF_PROGRESS_LOADING));
  } else {
    NOTREACHED();
    return PP_ERROR_FAILED;
  }

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
  instance->GetRenderView()->Send(
      new ChromeViewHostMsg_PDFUpdateContentRestrictions(
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
  render_view->Send(new ChromeViewHostMsg_PDFHasUnsupportedFeature(
      render_view->GetRoutingID()));
  return PP_OK;
}

int32_t PepperPDFHost::OnHostMsgPrint(
    ppapi::host::HostMessageContext* context) {
#if defined(ENABLE_FULL_PRINTING)
  content::PepperPluginInstance* instance =
      host_->GetPluginInstance(pp_instance());
  if (!instance)
    return PP_ERROR_FAILED;

  blink::WebElement element = instance->GetContainer()->element();
  blink::WebView* view = element.document().frame()->view();
  content::RenderView* render_view = content::RenderView::FromWebView(view);

  using printing::PrintWebViewHelper;
  PrintWebViewHelper* print_view_helper = PrintWebViewHelper::Get(render_view);
  if (print_view_helper) {
    print_view_helper->PrintNode(element);
    return PP_OK;
  }
#endif
  return PP_ERROR_FAILED;
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
  render_view->Send(new ChromeViewHostMsg_PDFSaveURLAs(
      render_view->GetRoutingID(), url, referrer));
  return PP_OK;
}

int32_t PepperPDFHost::OnHostMsgGetResourceImage(
    ppapi::host::HostMessageContext* context,
    PP_ResourceImage image_id,
    float scale) {
  int res_id = 0;
  for (size_t i = 0; i < arraysize(kResourceImageMap); ++i) {
    if (kResourceImageMap[i].pp_id == image_id) {
      res_id = kResourceImageMap[i].res_id;
      break;
    }
  }
  if (res_id == 0)
    return PP_ERROR_FAILED;

  gfx::ImageSkia* res_image_skia =
      ResourceBundle::GetSharedInstance().GetImageSkiaNamed(res_id);

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
