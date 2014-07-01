// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/pepper/ppb_pdf_impl.h"

#include "base/files/scoped_file.h"
#include "base/metrics/histogram.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/common/render_messages.h"
#include "chrome/renderer/printing/print_web_view_helper.h"
#include "content/public/common/child_process_sandbox_support_linux.h"
#include "content/public/common/referrer.h"
#include "content/public/renderer/pepper_plugin_instance.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "grit/webkit_resources.h"
#include "grit/webkit_strings.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/private/ppb_pdf.h"
#include "ppapi/c/trusted/ppb_browser_font_trusted.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/shared_impl/resource_tracker.h"
#include "ppapi/shared_impl/var.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebPluginContainer.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "third_party/icu/source/i18n/unicode/usearch.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_ANDROID)
class PrivateFontFile : public ppapi::Resource {
 public:
  PrivateFontFile(PP_Instance instance, int fd)
      : Resource(ppapi::OBJECT_IS_IMPL, instance), fd_(fd) {}

  bool GetFontTable(uint32_t table, void* output, uint32_t* output_length) {
    size_t temp_size = static_cast<size_t>(*output_length);
    bool rv = content::GetFontTable(
        fd_.get(), table, 0 /* offset */, static_cast<uint8_t*>(output),
        &temp_size);
    *output_length = base::checked_cast<uint32_t>(temp_size);
    return rv;
  }

 protected:
  virtual ~PrivateFontFile() {}

 private:
  base::ScopedFD fd_;
};
#endif

struct ResourceImageInfo {
  PP_ResourceImage pp_id;
  int res_id;
};

static const ResourceImageInfo kResourceImageMap[] = {
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

#if defined(ENABLE_FULL_PRINTING)

blink::WebElement GetWebElement(PP_Instance instance_id) {
  content::PepperPluginInstance* instance =
      content::PepperPluginInstance::Get(instance_id);
  if (!instance)
    return blink::WebElement();
  return instance->GetContainer()->element();
}

printing::PrintWebViewHelper* GetPrintWebViewHelper(
    const blink::WebElement& element) {
  if (element.isNull())
    return NULL;
  blink::WebView* view = element.document().frame()->view();
  content::RenderView* render_view = content::RenderView::FromWebView(view);
  return printing::PrintWebViewHelper::Get(render_view);
}

bool IsPrintingEnabled(PP_Instance instance_id) {
  blink::WebElement element = GetWebElement(instance_id);
  printing::PrintWebViewHelper* helper = GetPrintWebViewHelper(element);
  return helper && helper->IsPrintingEnabled();
}

#else  // ENABLE_FULL_PRINTING

bool IsPrintingEnabled(PP_Instance instance_id) { return false; }

#endif  // ENABLE_FULL_PRINTING

PP_Var GetLocalizedString(PP_Instance instance_id,
                          PP_ResourceString string_id) {
  content::PepperPluginInstance* instance =
      content::PepperPluginInstance::Get(instance_id);
  if (!instance)
    return PP_MakeUndefined();

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
  }

  return ppapi::StringVar::StringToPPVar(rv);
}

PP_Resource GetFontFileWithFallback(
    PP_Instance instance_id,
    const PP_BrowserFont_Trusted_Description* description,
    PP_PrivateFontCharset charset) {
#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_ANDROID)
  // Validate the instance before using it below.
  if (!content::PepperPluginInstance::Get(instance_id))
    return 0;

  scoped_refptr<ppapi::StringVar> face_name(
      ppapi::StringVar::FromPPVar(description->face));
  if (!face_name)
    return 0;

  int fd = content::MatchFontWithFallback(
      face_name->value().c_str(),
      description->weight >= PP_BROWSERFONT_TRUSTED_WEIGHT_BOLD,
      description->italic,
      charset,
      description->family);
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
#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_ANDROID)
  ppapi::Resource* resource =
      ppapi::PpapiGlobals::Get()->GetResourceTracker()->GetResource(font_file);
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
  const base::char16* string =
      reinterpret_cast<const base::char16*>(input_string);
  const base::char16* term = reinterpret_cast<const base::char16*>(input_term);

  UErrorCode status = U_ZERO_ERROR;
  UStringSearch* searcher =
      usearch_open(term,
                   -1,
                   string,
                   -1,
                   content::RenderThread::Get()->GetLocale().c_str(),
                   0,
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
  content::PepperPluginInstance* instance =
      content::PepperPluginInstance::Get(instance_id);
  if (!instance)
    return;
  instance->GetRenderView()->DidStartLoading();
}

void DidStopLoading(PP_Instance instance_id) {
  content::PepperPluginInstance* instance =
      content::PepperPluginInstance::Get(instance_id);
  if (!instance)
    return;
  instance->GetRenderView()->DidStopLoading();
}

void SetContentRestriction(PP_Instance instance_id, int restrictions) {
  content::PepperPluginInstance* instance =
      content::PepperPluginInstance::Get(instance_id);
  if (!instance)
    return;
  instance->GetRenderView()->Send(
      new ChromeViewHostMsg_PDFUpdateContentRestrictions(
          instance->GetRenderView()->GetRoutingID(), restrictions));
}

void HistogramPDFPageCount(PP_Instance instance, int count) {
  UMA_HISTOGRAM_COUNTS_10000("PDF.PageCount", count);
}

void UserMetricsRecordAction(PP_Instance instance, PP_Var action) {
  scoped_refptr<ppapi::StringVar> action_str(
      ppapi::StringVar::FromPPVar(action));
  if (action_str)
    content::RenderThread::Get()->RecordComputedAction(action_str->value());
}

void HasUnsupportedFeature(PP_Instance instance_id) {
  content::PepperPluginInstance* instance =
      content::PepperPluginInstance::Get(instance_id);
  if (!instance)
    return;

  // Only want to show an info bar if the pdf is the whole tab.
  if (!instance->IsFullPagePlugin())
    return;

  blink::WebView* view =
      instance->GetContainer()->element().document().frame()->view();
  content::RenderView* render_view = content::RenderView::FromWebView(view);
  render_view->Send(new ChromeViewHostMsg_PDFHasUnsupportedFeature(
      render_view->GetRoutingID()));
}

void SaveAs(PP_Instance instance_id) {
  content::PepperPluginInstance* instance =
      content::PepperPluginInstance::Get(instance_id);
  if (!instance)
    return;
  GURL url = instance->GetPluginURL();

  content::RenderView* render_view = instance->GetRenderView();
  blink::WebLocalFrame* frame =
      render_view->GetWebView()->mainFrame()->toWebLocalFrame();
  content::Referrer referrer(frame->document().url(),
                             frame->document().referrerPolicy());
  render_view->Send(new ChromeViewHostMsg_PDFSaveURLAs(
      render_view->GetRoutingID(), url, referrer));
}

PP_Bool IsFeatureEnabled(PP_Instance instance, PP_PDFFeature feature) {
  switch (feature) {
    case PP_PDFFEATURE_HIDPI:
      return PP_TRUE;
    case PP_PDFFEATURE_PRINTING:
      return IsPrintingEnabled(instance) ? PP_TRUE : PP_FALSE;
  }
  return PP_FALSE;
}

PP_Resource GetResourceImageForScale(PP_Instance instance_id,
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
    return 0;

  // Validate the instance.
  content::PepperPluginInstance* instance =
      content::PepperPluginInstance::Get(instance_id);
  if (!instance)
    return 0;

  gfx::ImageSkia* res_image_skia =
      ResourceBundle::GetSharedInstance().GetImageSkiaNamed(res_id);

  if (!res_image_skia)
    return 0;

  return instance->CreateImage(res_image_skia, scale);
}

PP_Resource GetResourceImage(PP_Instance instance_id,
                             PP_ResourceImage image_id) {
  return GetResourceImageForScale(instance_id, image_id, 1.0f);
}

PP_Var ModalPromptForPassword(PP_Instance instance_id, PP_Var message) {
  content::PepperPluginInstance* instance =
      content::PepperPluginInstance::Get(instance_id);
  if (!instance)
    return PP_MakeUndefined();

  std::string actual_value;
  scoped_refptr<ppapi::StringVar> message_string(
      ppapi::StringVar::FromPPVar(message));

  IPC::SyncMessage* msg = new ChromeViewHostMsg_PDFModalPromptForPassword(
      instance->GetRenderView()->GetRoutingID(),
      message_string->value(),
      &actual_value);
  msg->EnableMessagePumping();
  instance->GetRenderView()->Send(msg);

  return ppapi::StringVar::StringToPPVar(actual_value);
}

PP_Bool IsOutOfProcess(PP_Instance instance_id) { return PP_FALSE; }

void SetSelectedText(PP_Instance instance_id, const char* selected_text) {
  // This function is intended for out of process PDF plugin.
}

void SetLinkUnderCursor(PP_Instance instance_id, const char* url) {
  content::PepperPluginInstance* instance =
      content::PepperPluginInstance::Get(instance_id);
  if (!instance)
    return;
  instance->SetLinkUnderCursor(url);
}

const PPB_PDF ppb_pdf = {                      //
    &GetLocalizedString,                       //
    &GetResourceImage,                         //
    &GetFontFileWithFallback,                  //
    &GetFontTableForPrivateFontFile,           //
    &SearchString,                             //
    &DidStartLoading,                          //
    &DidStopLoading,                           //
    &SetContentRestriction,                    //
    &HistogramPDFPageCount,                    //
    &UserMetricsRecordAction,                  //
    &HasUnsupportedFeature,                    //
    &SaveAs,                                   //
    &PPB_PDF_Impl::InvokePrintingForInstance,  //
    &IsFeatureEnabled,                         //
    &GetResourceImageForScale,                 //
    &ModalPromptForPassword,                   //
    &IsOutOfProcess,                           //
    &SetSelectedText,                          //
    &SetLinkUnderCursor,                       //
};

}  // namespace

// static
const PPB_PDF* PPB_PDF_Impl::GetInterface() { return &ppb_pdf; }

// static
void PPB_PDF_Impl::InvokePrintingForInstance(PP_Instance instance_id) {
#if defined(ENABLE_FULL_PRINTING)
  blink::WebElement element = GetWebElement(instance_id);
  printing::PrintWebViewHelper* helper = GetPrintWebViewHelper(element);
  if (helper)
    helper->PrintNode(element);
#endif  // ENABLE_FULL_PRINTING
}
