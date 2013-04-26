// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/pepper/pepper_pdf_host.h"

#include "base/utf_string_conversions.h"
#include "chrome/common/render_messages.h"
#include "chrome/renderer/printing/print_web_view_helper.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "content/public/renderer/renderer_ppapi_host.h"
#include "grit/webkit_resources.h"
#include "grit/webkit_strings.h"
#include "ppapi/host/dispatch_host_message.h"
#include "ppapi/host/host_message_context.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/proxy/host_dispatcher.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/ppb_image_data_proxy.h"
#include "ppapi/shared_impl/scoped_pp_resource.h"
#include "ppapi/thunk/enter.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPluginContainer.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/layout.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/gfx/point.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"
#include "webkit/plugins/ppapi/ppb_image_data_impl.h"

using webkit::ppapi::PluginInstance;

namespace chrome {

namespace {

struct ResourceImageInfo {
  PP_ResourceImage pp_id;
  int res_id;
};

static const ResourceImageInfo kResourceImageMap[] = {
  { PP_RESOURCEIMAGE_PDF_BUTTON_FTP, IDR_PDF_BUTTON_FTP },
  { PP_RESOURCEIMAGE_PDF_BUTTON_FTP_HOVER, IDR_PDF_BUTTON_FTP_HOVER },
  { PP_RESOURCEIMAGE_PDF_BUTTON_FTP_PRESSED, IDR_PDF_BUTTON_FTP_PRESSED },
  { PP_RESOURCEIMAGE_PDF_BUTTON_FTW, IDR_PDF_BUTTON_FTW },
  { PP_RESOURCEIMAGE_PDF_BUTTON_FTW_HOVER, IDR_PDF_BUTTON_FTW_HOVER },
  { PP_RESOURCEIMAGE_PDF_BUTTON_FTW_PRESSED, IDR_PDF_BUTTON_FTW_PRESSED },
  { PP_RESOURCEIMAGE_PDF_BUTTON_ZOOMIN_END, IDR_PDF_BUTTON_ZOOMIN_END },
  { PP_RESOURCEIMAGE_PDF_BUTTON_ZOOMIN_END_HOVER,
      IDR_PDF_BUTTON_ZOOMIN_END_HOVER },
  { PP_RESOURCEIMAGE_PDF_BUTTON_ZOOMIN_END_PRESSED,
      IDR_PDF_BUTTON_ZOOMIN_END_PRESSED },
  { PP_RESOURCEIMAGE_PDF_BUTTON_ZOOMIN, IDR_PDF_BUTTON_ZOOMIN },
  { PP_RESOURCEIMAGE_PDF_BUTTON_ZOOMIN_HOVER, IDR_PDF_BUTTON_ZOOMIN_HOVER },
  { PP_RESOURCEIMAGE_PDF_BUTTON_ZOOMIN_PRESSED, IDR_PDF_BUTTON_ZOOMIN_PRESSED },
  { PP_RESOURCEIMAGE_PDF_BUTTON_ZOOMOUT, IDR_PDF_BUTTON_ZOOMOUT },
  { PP_RESOURCEIMAGE_PDF_BUTTON_ZOOMOUT_HOVER, IDR_PDF_BUTTON_ZOOMOUT_HOVER },
  { PP_RESOURCEIMAGE_PDF_BUTTON_ZOOMOUT_PRESSED,
      IDR_PDF_BUTTON_ZOOMOUT_PRESSED },
  { PP_RESOURCEIMAGE_PDF_BUTTON_SAVE, IDR_PDF_BUTTON_SAVE },
  { PP_RESOURCEIMAGE_PDF_BUTTON_SAVE_HOVER, IDR_PDF_BUTTON_SAVE_HOVER },
  { PP_RESOURCEIMAGE_PDF_BUTTON_SAVE_PRESSED, IDR_PDF_BUTTON_SAVE_PRESSED },
  { PP_RESOURCEIMAGE_PDF_BUTTON_PRINT, IDR_PDF_BUTTON_PRINT },
  { PP_RESOURCEIMAGE_PDF_BUTTON_PRINT_HOVER, IDR_PDF_BUTTON_PRINT_HOVER },
  { PP_RESOURCEIMAGE_PDF_BUTTON_PRINT_PRESSED, IDR_PDF_BUTTON_PRINT_PRESSED },
  { PP_RESOURCEIMAGE_PDF_BUTTON_PRINT_DISABLED, IDR_PDF_BUTTON_PRINT_DISABLED },
  { PP_RESOURCEIMAGE_PDF_BUTTON_THUMBNAIL_0, IDR_PDF_THUMBNAIL_0 },
  { PP_RESOURCEIMAGE_PDF_BUTTON_THUMBNAIL_1, IDR_PDF_THUMBNAIL_1 },
  { PP_RESOURCEIMAGE_PDF_BUTTON_THUMBNAIL_2, IDR_PDF_THUMBNAIL_2 },
  { PP_RESOURCEIMAGE_PDF_BUTTON_THUMBNAIL_3, IDR_PDF_THUMBNAIL_3 },
  { PP_RESOURCEIMAGE_PDF_BUTTON_THUMBNAIL_4, IDR_PDF_THUMBNAIL_4 },
  { PP_RESOURCEIMAGE_PDF_BUTTON_THUMBNAIL_5, IDR_PDF_THUMBNAIL_5 },
  { PP_RESOURCEIMAGE_PDF_BUTTON_THUMBNAIL_6, IDR_PDF_THUMBNAIL_6 },
  { PP_RESOURCEIMAGE_PDF_BUTTON_THUMBNAIL_7, IDR_PDF_THUMBNAIL_7 },
  { PP_RESOURCEIMAGE_PDF_BUTTON_THUMBNAIL_8, IDR_PDF_THUMBNAIL_8 },
  { PP_RESOURCEIMAGE_PDF_BUTTON_THUMBNAIL_9, IDR_PDF_THUMBNAIL_9 },
  { PP_RESOURCEIMAGE_PDF_BUTTON_THUMBNAIL_NUM_BACKGROUND,
      IDR_PDF_THUMBNAIL_NUM_BACKGROUND },
  { PP_RESOURCEIMAGE_PDF_PROGRESS_BAR_0, IDR_PDF_PROGRESS_BAR_0 },
  { PP_RESOURCEIMAGE_PDF_PROGRESS_BAR_1, IDR_PDF_PROGRESS_BAR_1 },
  { PP_RESOURCEIMAGE_PDF_PROGRESS_BAR_2, IDR_PDF_PROGRESS_BAR_2 },
  { PP_RESOURCEIMAGE_PDF_PROGRESS_BAR_3, IDR_PDF_PROGRESS_BAR_3 },
  { PP_RESOURCEIMAGE_PDF_PROGRESS_BAR_4, IDR_PDF_PROGRESS_BAR_4 },
  { PP_RESOURCEIMAGE_PDF_PROGRESS_BAR_5, IDR_PDF_PROGRESS_BAR_5 },
  { PP_RESOURCEIMAGE_PDF_PROGRESS_BAR_6, IDR_PDF_PROGRESS_BAR_6 },
  { PP_RESOURCEIMAGE_PDF_PROGRESS_BAR_7, IDR_PDF_PROGRESS_BAR_7 },
  { PP_RESOURCEIMAGE_PDF_PROGRESS_BAR_8, IDR_PDF_PROGRESS_BAR_8 },
  { PP_RESOURCEIMAGE_PDF_PROGRESS_BAR_BACKGROUND,
      IDR_PDF_PROGRESS_BAR_BACKGROUND },
  { PP_RESOURCEIMAGE_PDF_PAGE_INDICATOR_BACKGROUND,
      IDR_PDF_PAGE_INDICATOR_BACKGROUND },
  { PP_RESOURCEIMAGE_PDF_PAGE_DROPSHADOW, IDR_PDF_PAGE_DROPSHADOW },
  { PP_RESOURCEIMAGE_PDF_PAN_SCROLL_ICON, IDR_PAN_SCROLL_ICON },
};

// Valid strings for user metrics actions.
static const char* kValidUserMetricsActions[] = {
  "PDF.PrintPage",
  "PDF.ZoomFromBrowser",
  "PDF.FitToPageButton",
  "PDF.FitToWidthButton",
  "PDF.ZoomOutButton",
  "PDF.ZoomInButton",
  "PDF.SaveButton",
  "PDF.PrintButton",
  "PDF.LoadSuccess",
  "PDF.LoadFailure",
  "PDF.PreviewDocumentLoadFailure",
};

}  // namespace

PepperPDFHost::PepperPDFHost(
    content::RendererPpapiHost* host,
    PP_Instance instance,
    PP_Resource resource)
    : ppapi::host::ResourceHost(host->GetPpapiHost(), instance, resource),
      host_(host) {
}

PepperPDFHost::~PepperPDFHost() {
}

int32_t PepperPDFHost::OnResourceMessageReceived(
    const IPC::Message& msg,
    ppapi::host::HostMessageContext* context) {
  IPC_BEGIN_MESSAGE_MAP(PepperPDFHost, msg)
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
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(PpapiHostMsg_PDF_Print,
                                      OnHostMsgPrint)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(PpapiHostMsg_PDF_SaveAs,
                                      OnHostMsgSaveAs)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_PDF_GetResourceImage,
                                      OnHostMsgGetResourceImage)
  IPC_END_MESSAGE_MAP()
  return PP_ERROR_FAILED;
}

int32_t PepperPDFHost::OnHostMsgGetLocalizedString(
    ppapi::host::HostMessageContext* context,
    PP_ResourceString string_id) {
  std::string rv;
  if (string_id == PP_RESOURCESTRING_PDFGETPASSWORD) {
    rv = UTF16ToUTF8(l10n_util::GetStringUTF16(IDS_PDF_NEED_PASSWORD));
  } else if (string_id == PP_RESOURCESTRING_PDFLOADING) {
    rv = UTF16ToUTF8(l10n_util::GetStringUTF16(IDS_PDF_PAGE_LOADING));
  } else if (string_id == PP_RESOURCESTRING_PDFLOAD_FAILED) {
    rv = UTF16ToUTF8(l10n_util::GetStringUTF16(IDS_PDF_PAGE_LOAD_FAILED));
  } else if (string_id == PP_RESOURCESTRING_PDFPROGRESSLOADING) {
    rv = UTF16ToUTF8(l10n_util::GetStringUTF16(IDS_PDF_PROGRESS_LOADING));
  } else {
    NOTREACHED();
    return PP_ERROR_FAILED;
  }

  context->reply_msg = PpapiPluginMsg_PDF_GetLocalizedStringReply(rv);
  return PP_OK;
}

int32_t PepperPDFHost::OnHostMsgDidStartLoading(
    ppapi::host::HostMessageContext* context) {
  PluginInstance* instance = host_->GetPluginInstance(pp_instance());
  if (!instance)
    return PP_ERROR_FAILED;
  instance->delegate()->DidStartLoading();
  return PP_OK;
}

int32_t PepperPDFHost::OnHostMsgDidStopLoading(
    ppapi::host::HostMessageContext* context) {
  PluginInstance* instance = host_->GetPluginInstance(pp_instance());
  if (!instance)
    return PP_ERROR_FAILED;
  instance->delegate()->DidStopLoading();
  return PP_OK;
}

int32_t PepperPDFHost::OnHostMsgSetContentRestriction(
    ppapi::host::HostMessageContext* context, int restrictions) {
  PluginInstance* instance = host_->GetPluginInstance(pp_instance());
  if (!instance)
    return PP_ERROR_FAILED;
  instance->delegate()->SetContentRestriction(restrictions);
  return PP_OK;
}

int32_t PepperPDFHost::OnHostMsgUserMetricsRecordAction(
    ppapi::host::HostMessageContext* context,
    const std::string& action) {
  bool valid = false;
  for (size_t i = 0; i < arraysize(kResourceImageMap); ++i) {
    if (action == kValidUserMetricsActions[i]) {
      valid = true;
      break;
    }
  }
  if (!valid) {
    NOTREACHED();
    return PP_ERROR_FAILED;
  }
  content::RenderThread::Get()->RecordUserMetrics(action);
  return PP_OK;
}

int32_t PepperPDFHost::OnHostMsgHasUnsupportedFeature(
    ppapi::host::HostMessageContext* context) {
  PluginInstance* instance = host_->GetPluginInstance(pp_instance());
  if (!instance)
    return PP_ERROR_FAILED;

  // Only want to show an info bar if the pdf is the whole tab.
  if (!instance->IsFullPagePlugin())
    return PP_OK;

  WebKit::WebView* view =
      instance->container()->element().document().frame()->view();
  content::RenderView* render_view = content::RenderView::FromWebView(view);
  render_view->Send(new ChromeViewHostMsg_PDFHasUnsupportedFeature(
      render_view->GetRoutingID()));
  return PP_OK;
}

int32_t PepperPDFHost::OnHostMsgPrint(
    ppapi::host::HostMessageContext* context) {
#if defined(ENABLE_PRINTING)
  PluginInstance* instance = host_->GetPluginInstance(pp_instance());
  if (!instance)
    return PP_ERROR_FAILED;

  WebKit::WebElement element = instance->container()->element();
  WebKit::WebView* view = element.document().frame()->view();
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
  PluginInstance* instance = host_->GetPluginInstance(pp_instance());
  if (!instance)
    return PP_ERROR_FAILED;
  instance->delegate()->SaveURLAs(instance->plugin_url());
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

  ui::ScaleFactor scale_factor = ui::GetScaleFactorFromScale(scale);

  gfx::ImageSkia* res_image_skia =
      ResourceBundle::GetSharedInstance().GetImageSkiaNamed(res_id);

  if (!res_image_skia)
    return PP_ERROR_FAILED;

  gfx::ImageSkiaRep image_skia_rep = res_image_skia->GetRepresentation(
      scale_factor);

  if (image_skia_rep.is_null() || image_skia_rep.scale_factor() != scale_factor)
    return PP_ERROR_FAILED;

  PP_Size pp_size;
  pp_size.width = image_skia_rep.pixel_width();
  pp_size.height = image_skia_rep.pixel_height();

  ppapi::HostResource host_resource;
  std::string image_data_desc;
  ppapi::proxy::ImageHandle image_handle;
  uint32_t byte_count = 0;
  bool success = CreateImageData(
      pp_instance(),
      webkit::ppapi::PPB_ImageData_Impl::GetNativeImageDataFormat(),
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
  // This mess of #defines is needed to translate between ImageHandles and
  // SerializedHandles. This is something that should be addressed with a
  // refactoring of PPB_ImageData.
#if defined(OS_WIN)
  ppapi::proxy::SerializedHandle serialized_handle;
  PpapiPluginMsg_PDF_GetResourceImageReply reply_msg(host_resource,
                                                     image_data_desc,
                                                     0);
  ppapi::proxy::HostDispatcher* dispatcher =
      ppapi::proxy::HostDispatcher::GetForInstance(pp_instance());
  if (!dispatcher)
    return PP_ERROR_FAILED;
  serialized_handle.set_shmem(
      dispatcher->ShareHandleWithRemote(image_handle, false), byte_count);
  reply_context.params.AppendHandle(serialized_handle);
#elif defined(OS_MACOSX)
  ppapi::proxy::SerializedHandle serialized_handle;
  PpapiPluginMsg_PDF_GetResourceImageReply reply_msg(host_resource,
                                                     image_data_desc,
                                                     0);
  serialized_handle.set_shmem(image_handle, byte_count);
  reply_context.params.AppendHandle(serialized_handle);
#elif defined(OS_LINUX)
  // For linux, we pass the SysV shared memory key in the message.
  PpapiPluginMsg_PDF_GetResourceImageReply reply_msg(host_resource,
                                                     image_data_desc,
                                                     image_handle);
#else
  // Not supported on the other platforms.
  // This is a stub reply_msg not to break the build.
  PpapiPluginMsg_PDF_GetResourceImageReply reply_msg(host_resource,
                                                     image_data_desc,
                                                     0);
  NOTIMPLEMENTED();
  return PP_ERROR_NOTSUPPORTED;
#endif

  SendReply(reply_context, reply_msg);

  // Keep a reference to the resource only if the function succeeds.
  image_data_resource.Release();

  return PP_OK_COMPLETIONPENDING;
}

// TODO(raymes): This function is mainly copied from ppb_image_data_proxy.cc.
// It's a mess and needs to be fixed in several ways but this is better done
// when we refactor PPB_ImageData. On success, the serialized handle will be
// non-null.
bool PepperPDFHost::CreateImageData(
    PP_Instance instance,
    PP_ImageDataFormat format,
    const PP_Size& size,
    const SkBitmap& pixels_to_write,
    ppapi::HostResource* result,
    std::string* out_image_data_desc,
    ppapi::proxy::ImageHandle* out_image_handle,
    uint32_t* out_byte_count) {
  // Create the resource.
  ppapi::thunk::EnterResourceCreation enter(instance);
  if (enter.failed())
    return false;

  PP_Resource resource = enter.functions()->CreateImageData(instance, format,
                                                            &size, PP_FALSE);
  if (!resource)
    return false;
  result->SetHostResource(instance, resource);

  // Write the image to the resource shared memory.
  ppapi::thunk::EnterResourceNoLock<ppapi::thunk::PPB_ImageData_API>
      enter_resource(resource, false);
  if (enter_resource.failed())
    return false;

  webkit::ppapi::PPB_ImageData_Impl* image_data =
      static_cast<webkit::ppapi::PPB_ImageData_Impl*>(enter_resource.object());
  webkit::ppapi::ImageDataAutoMapper mapper(image_data);
  if (!mapper.is_valid())
    return false;

  skia::PlatformCanvas* canvas = image_data->GetPlatformCanvas();
  // Note: Do not skBitmap::copyTo the canvas bitmap directly because it will
  // ignore the allocated pixels in shared memory and re-allocate a new buffer.
  canvas->writePixels(pixels_to_write, 0, 0);

  // Get the image description, it's just serialized as a string.
  PP_ImageDataDesc desc;
  if (enter_resource.object()->Describe(&desc) == PP_TRUE) {
    out_image_data_desc->resize(sizeof(PP_ImageDataDesc));
    memcpy(&(*out_image_data_desc)[0], &desc, sizeof(PP_ImageDataDesc));
  } else {
    return false;
  }

  // Get the shared memory handle.
  int32_t handle = 0;
  if (enter_resource.object()->GetSharedMemory(
      &handle, out_byte_count) != PP_OK) {
    return false;
  }

  *out_image_handle = ppapi::proxy::ImageData::HandleFromInt(handle);
  return true;
}

}  // namespace chrome
