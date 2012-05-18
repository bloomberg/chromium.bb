// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/pepper/ppb_pdf_impl.h"

#include "base/metrics/histogram.h"
#include "base/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/common/render_messages.h"
#include "chrome/renderer/print_web_view_helper.h"
#include "content/public/common/child_process_sandbox_support_linux.h"
#include "content/public/common/content_client.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "grit/webkit_resources.h"
#include "grit/webkit_strings.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/private/ppb_pdf.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/shared_impl/resource_tracker.h"
#include "ppapi/shared_impl/var.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPluginContainer.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "unicode/usearch.h"
#include "webkit/plugins/ppapi/host_globals.h"
#include "webkit/plugins/ppapi/plugin_delegate.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"
#include "webkit/plugins/ppapi/ppb_image_data_impl.h"

using ppapi::PpapiGlobals;
using webkit::ppapi::HostGlobals;
using webkit::ppapi::PluginInstance;
using WebKit::WebElement;
using WebKit::WebView;
using content::RenderThread;

namespace {

#if defined(OS_LINUX) || defined(OS_OPENBSD)
class PrivateFontFile : public ppapi::Resource {
 public:
  PrivateFontFile(PP_Instance instance, int fd)
      : Resource(ppapi::OBJECT_IS_IMPL, instance),
        fd_(fd) {
  }

  bool GetFontTable(uint32_t table,
                    void* output,
                    uint32_t* output_length) {
    size_t temp_size = static_cast<size_t>(*output_length);
    bool rv = content::GetFontTable(
        fd_, table, static_cast<uint8_t*>(output), &temp_size);
    *output_length = static_cast<uint32_t>(temp_size);
    return rv;
  }

 protected:
  virtual ~PrivateFontFile() {}

 private:
  int fd_;
};
#endif

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

PP_Var GetLocalizedString(PP_Instance instance_id,
                          PP_ResourceString string_id) {
  PluginInstance* instance =
      content::GetHostGlobals()->GetInstance(instance_id);
  if (!instance)
    return PP_MakeUndefined();

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
  }

  return ppapi::StringVar::StringToPPVar(rv);
}

PP_Resource GetResourceImage(PP_Instance instance_id,
                             PP_ResourceImage image_id) {
  int res_id = 0;
  for (size_t i = 0; i < arraysize(kResourceImageMap); ++i) {
    if (kResourceImageMap[i].pp_id == image_id) {
      res_id = kResourceImageMap[i].res_id;
      break;
    }
  }
  if (res_id == 0)
    return 0;

  SkBitmap* res_bitmap =
      ResourceBundle::GetSharedInstance().GetBitmapNamed(res_id);

  // Validate the instance.
  if (!content::GetHostGlobals()->GetInstance(instance_id))
    return 0;
  scoped_refptr<webkit::ppapi::PPB_ImageData_Impl> image_data(
      new webkit::ppapi::PPB_ImageData_Impl(instance_id));
  if (!image_data->Init(
          webkit::ppapi::PPB_ImageData_Impl::GetNativeImageDataFormat(),
          res_bitmap->width(), res_bitmap->height(), false)) {
    return 0;
  }

  webkit::ppapi::ImageDataAutoMapper mapper(image_data);
  if (!mapper.is_valid())
    return 0;

  skia::PlatformCanvas* canvas = image_data->GetPlatformCanvas();
  // Note: Do not skBitmap::copyTo the canvas bitmap directly because it will
  // ignore the allocated pixels in shared memory and re-allocate a new buffer.
  canvas->writePixels(*res_bitmap, 0, 0);

  return image_data->GetReference();
}

PP_Resource GetFontFileWithFallback(
    PP_Instance instance_id,
    const PP_FontDescription_Dev* description,
    PP_PrivateFontCharset charset) {
#if defined(OS_LINUX) || defined(OS_OPENBSD)
  // Validate the instance before using it below.
  if (!content::GetHostGlobals()->GetInstance(instance_id))
    return 0;

  scoped_refptr<ppapi::StringVar> face_name(ppapi::StringVar::FromPPVar(
      description->face));
  if (!face_name)
    return 0;

  int fd = content::MatchFontWithFallback(
      face_name->value().c_str(),
      description->weight >= PP_FONTWEIGHT_BOLD,
      description->italic,
      charset);
  if (fd == -1)
    return 0;

  scoped_refptr<PrivateFontFile> font(new PrivateFontFile(instance_id, fd));

  return font->GetReference();
#else
  // For trusted PPAPI plugins, this is only needed in Linux since font loading
  // on Windows and Mac works through the renderer sandbox.
  return 0;
#endif
}

bool GetFontTableForPrivateFontFile(PP_Resource font_file,
                                    uint32_t table,
                                    void* output,
                                    uint32_t* output_length) {
#if defined(OS_LINUX) || defined(OS_OPENBSD)
  ppapi::Resource* resource =
      PpapiGlobals::Get()->GetResourceTracker()->GetResource(font_file);
  if (!resource)
    return false;

  PrivateFontFile* font = static_cast<PrivateFontFile*>(resource);
  return font->GetFontTable(table, output, output_length);
#else
  return false;
#endif
}

void SearchString(PP_Instance instance,
                  const unsigned short* input_string,
                  const unsigned short* input_term,
                  bool case_sensitive,
                  PP_PrivateFindResult** results,
                  int* count) {
  const char16* string = reinterpret_cast<const char16*>(input_string);
  const char16* term = reinterpret_cast<const char16*>(input_term);

  UErrorCode status = U_ZERO_ERROR;
  UStringSearch* searcher = usearch_open(
      term, -1, string, -1, RenderThread::Get()->GetLocale().c_str(), 0,
      &status);
  DCHECK(status == U_ZERO_ERROR || status == U_USING_FALLBACK_WARNING ||
         status == U_USING_DEFAULT_WARNING);
  UCollationStrength strength = case_sensitive ? UCOL_TERTIARY : UCOL_PRIMARY;

  UCollator* collator = usearch_getCollator(searcher);
  if (ucol_getStrength(collator) != strength) {
    ucol_setStrength(collator, strength);
    usearch_reset(searcher);
  }

  status = U_ZERO_ERROR;
  int match_start = usearch_first(searcher, &status);
  DCHECK(status == U_ZERO_ERROR);

  std::vector<PP_PrivateFindResult> pp_results;
  while (match_start != USEARCH_DONE) {
    size_t matched_length = usearch_getMatchedLength(searcher);
    PP_PrivateFindResult result;
    result.start_index = match_start;
    result.length = matched_length;
    pp_results.push_back(result);
    match_start = usearch_next(searcher, &status);
    DCHECK(status == U_ZERO_ERROR);
  }

  *count = pp_results.size();
  if (*count) {
    *results = reinterpret_cast<PP_PrivateFindResult*>(
        malloc(*count * sizeof(PP_PrivateFindResult)));
    memcpy(*results, &pp_results[0], *count * sizeof(PP_PrivateFindResult));
  } else {
    *results = NULL;
  }

  usearch_close(searcher);
}

void DidStartLoading(PP_Instance instance_id) {
  PluginInstance* instance =
      content::GetHostGlobals()->GetInstance(instance_id);
  if (!instance)
    return;
  instance->delegate()->DidStartLoading();
}

void DidStopLoading(PP_Instance instance_id) {
  PluginInstance* instance =
      content::GetHostGlobals()->GetInstance(instance_id);
  if (!instance)
    return;
  instance->delegate()->DidStopLoading();
}

void SetContentRestriction(PP_Instance instance_id, int restrictions) {
  PluginInstance* instance =
      content::GetHostGlobals()->GetInstance(instance_id);
  if (!instance)
    return;
  instance->delegate()->SetContentRestriction(restrictions);
}

void HistogramPDFPageCount(int count) {
  UMA_HISTOGRAM_COUNTS_10000("PDF.PageCount", count);
}

void UserMetricsRecordAction(PP_Var action) {
  scoped_refptr<ppapi::StringVar> action_str(
      ppapi::StringVar::FromPPVar(action));
  if (action_str)
    RenderThread::Get()->RecordUserMetrics(action_str->value());
}

void HasUnsupportedFeature(PP_Instance instance_id) {
  PluginInstance* instance =
      content::GetHostGlobals()->GetInstance(instance_id);
  if (!instance)
    return;

  // Only want to show an info bar if the pdf is the whole tab.
  if (!instance->IsFullPagePlugin())
    return;

  WebView* view = instance->container()->element().document().frame()->view();
  content::RenderView* render_view = content::RenderView::FromWebView(view);
  render_view->Send(new ChromeViewHostMsg_PDFHasUnsupportedFeature(
      render_view->GetRoutingID()));
}

void SaveAs(PP_Instance instance_id) {
  PluginInstance* instance =
      content::GetHostGlobals()->GetInstance(instance_id);
  if (!instance)
    return;
  instance->delegate()->SaveURLAs(instance->plugin_url());
}

const PPB_PDF ppb_pdf = {
  &GetLocalizedString,
  &GetResourceImage,
  &GetFontFileWithFallback,
  &GetFontTableForPrivateFontFile,
  &SearchString,
  &DidStartLoading,
  &DidStopLoading,
  &SetContentRestriction,
  &HistogramPDFPageCount,
  &UserMetricsRecordAction,
  &HasUnsupportedFeature,
  &SaveAs,
  &PPB_PDF_Impl::InvokePrintingForInstance
};

}  // namespace

// static
const PPB_PDF* PPB_PDF_Impl::GetInterface() {
  return &ppb_pdf;
}

// static
void PPB_PDF_Impl::InvokePrintingForInstance(PP_Instance instance_id) {
  PluginInstance* instance = HostGlobals::Get()->GetInstance(instance_id);
  if (!instance)
    return;

  WebKit::WebElement element = instance->container()->element();
  WebKit::WebView* view = element.document().frame()->view();
  content::RenderView* render_view = content::RenderView::FromWebView(view);

  PrintWebViewHelper* print_view_helper = PrintWebViewHelper::Get(render_view);
  if (print_view_helper)
    print_view_helper->PrintNode(element);
}
