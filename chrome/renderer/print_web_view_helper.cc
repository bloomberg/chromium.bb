// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/print_web_view_helper.h"

#if defined(OS_MACOSX) && !defined(USE_SKIA)
#include <CoreGraphics/CGContext.h>
#endif

#include <string>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/print_messages.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"
#include "chrome/renderer/prerender/prerender_helper.h"
#include "content/renderer/render_view.h"
#include "grit/generated_resources.h"
#include "printing/metafile_impl.h"
#include "printing/print_job_constants.h"
#include "printing/units.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCanvas.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebConsoleMessage.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDataSource.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSize.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURLRequest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURLResponse.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_POSIX)
#include "base/process_util.h"
#include "content/common/view_messages.h"
#endif

#if defined(USE_SKIA)
#include "skia/ext/vector_canvas.h"
#include "skia/ext/vector_platform_device_skia.h"
#include "third_party/skia/include/core/SkTypeface.h"
#elif defined(OS_MACOSX)
#include "base/mac/scoped_cftyperef.h"
#include "base/sys_string_conversions.h"
#include "ui/gfx/scoped_cg_context_save_gstate_mac.h"
#endif

#if defined(OS_MACOSX)
using base::mac::ScopedCFTypeRef;
#endif
using printing::ConvertPixelsToPoint;
using printing::ConvertPixelsToPointDouble;
using printing::ConvertUnit;
using printing::ConvertUnitDouble;
using printing::GetHeaderFooterSegmentWidth;
using WebKit::WebConsoleMessage;
using WebKit::WebDocument;
using WebKit::WebElement;
using WebKit::WebFrame;
using WebKit::WebNode;
using WebKit::WebSize;
using WebKit::WebString;
using WebKit::WebURLRequest;
using WebKit::WebView;

namespace {

#if defined(USE_SKIA)
typedef SkPaint HeaderFooterPaint;
#elif defined(OS_MACOSX)
typedef CFDictionaryRef HeaderFooterPaint;
#endif

const double kMinDpi = 1.0;

#if defined(OS_MACOSX) && !defined(USE_SKIA)
const double kBlackGrayLevel = 0.0;
const double kOpaqueLevel = 1.0;
#endif  // OS_MACOSX && !USE_SKIA

int GetDPI(const PrintMsg_Print_Params* print_params) {
#if defined(OS_MACOSX)
  // On the Mac, the printable area is in points, don't do any scaling based
  // on dpi.
  return printing::kPointsPerInch;
#else
  return static_cast<int>(print_params->dpi);
#endif  // defined(OS_MACOSX)
}

bool PrintMsg_Print_Params_IsEmpty(const PrintMsg_Print_Params& params) {
  return !params.document_cookie && !params.desired_dpi && !params.max_shrink &&
         !params.min_shrink && !params.dpi && params.printable_size.IsEmpty() &&
         !params.selection_only && params.page_size.IsEmpty() &&
         !params.margin_top && !params.margin_left &&
         !params.supports_alpha_blend;
}

bool PrintMsg_Print_Params_IsEqual(
    const PrintMsg_PrintPages_Params& oldParams,
    const PrintMsg_PrintPages_Params& newParams) {
  return oldParams.params.desired_dpi == newParams.params.desired_dpi &&
         oldParams.params.max_shrink == newParams.params.max_shrink &&
         oldParams.params.min_shrink == newParams.params.min_shrink &&
         oldParams.params.dpi == newParams.params.dpi &&
         oldParams.params.printable_size == newParams.params.printable_size &&
         oldParams.params.selection_only == newParams.params.selection_only &&
         oldParams.params.page_size == newParams.params.page_size &&
         oldParams.params.margin_top == newParams.params.margin_top &&
         oldParams.params.margin_left == newParams.params.margin_left &&
         oldParams.params.supports_alpha_blend ==
             newParams.params.supports_alpha_blend &&
         oldParams.pages.size() == newParams.pages.size() &&
         oldParams.params.display_header_footer ==
             newParams.params.display_header_footer &&
         oldParams.params.date == newParams.params.date &&
         oldParams.params.title == newParams.params.title &&
         oldParams.params.url == newParams.params.url &&
         std::equal(oldParams.pages.begin(), oldParams.pages.end(),
             newParams.pages.begin());
}

void CalculatePrintCanvasSize(const PrintMsg_Print_Params& print_params,
                              gfx::Size* result) {
  int dpi = GetDPI(&print_params);
  result->set_width(ConvertUnit(print_params.printable_size.width(), dpi,
                                print_params.desired_dpi));

  result->set_height(ConvertUnit(print_params.printable_size.height(), dpi,
                                 print_params.desired_dpi));
}

// Get the (x, y) coordinate from where printing of the current text should
// start depending on the horizontal alignment (LEFT, RIGHT, CENTER) and
// vertical alignment (TOP, BOTTOM).
SkPoint GetHeaderFooterPosition(
    float webkit_scale_factor,
    const PageSizeMargins& page_layout,
    printing::HorizontalHeaderFooterPosition horizontal_position,
    printing::VerticalHeaderFooterPosition vertical_position,
    double offset_to_baseline,
    double text_width_in_points) {
  SkScalar x = 0;
  switch (horizontal_position) {
    case printing::LEFT: {
      x = printing::kSettingHeaderFooterInterstice - page_layout.margin_left;
      break;
    }
    case printing::RIGHT: {
      x = page_layout.content_width + page_layout.margin_right -
          printing::kSettingHeaderFooterInterstice - text_width_in_points;
      break;
    }
    case printing::CENTER: {
      SkScalar available_width = GetHeaderFooterSegmentWidth(
          page_layout.margin_left + page_layout.margin_right +
              page_layout.content_width);
      x = available_width - page_layout.margin_left +
          (available_width - text_width_in_points) / 2;
      break;
    }
    default: {
      NOTREACHED();
    }
  }

  SkScalar y = 0;
  switch (vertical_position) {
    case printing::TOP:
      y = printing::kSettingHeaderFooterInterstice -
          page_layout.margin_top - offset_to_baseline;
      break;
    case printing::BOTTOM:
      y = page_layout.margin_bottom + page_layout.content_height -
          printing::kSettingHeaderFooterInterstice - offset_to_baseline;
      break;
    default:
      NOTREACHED();
  }

  SkPoint point = SkPoint::Make(x / webkit_scale_factor,
                                y / webkit_scale_factor);
  return point;
}

// Given a text, the positions, and the paint object, this method gets the
// coordinates and prints the text at those coordinates on the canvas.
void PrintHeaderFooterText(
    string16 text,
    WebKit::WebCanvas* canvas,
    HeaderFooterPaint paint,
    float webkit_scale_factor,
    const PageSizeMargins& page_layout,
    printing::HorizontalHeaderFooterPosition horizontal_position,
    printing::VerticalHeaderFooterPosition vertical_position,
    double offset_to_baseline) {
#if defined(USE_SKIA)
  size_t text_byte_length = text.length() * sizeof(char16);
  double text_width_in_points = SkScalarToDouble(paint.measureText(
      text.c_str(), text_byte_length));
  SkPoint point = GetHeaderFooterPosition(webkit_scale_factor, page_layout,
                                          horizontal_position,
                                          vertical_position, offset_to_baseline,
                                          text_width_in_points);
  paint.setTextSize(SkDoubleToScalar(
      paint.getTextSize() / webkit_scale_factor));
  canvas->drawText(text.c_str(), text_byte_length, point.x(), point.y(),
                   paint);
#elif defined(OS_MACOSX)
  ScopedCFTypeRef<CFStringRef> cf_text(base::SysUTF16ToCFStringRef(text));
  ScopedCFTypeRef<CFAttributedStringRef> cf_attr_text(
      CFAttributedStringCreate(NULL, cf_text, paint));
  ScopedCFTypeRef<CTLineRef> line(CTLineCreateWithAttributedString(
      cf_attr_text));
  double text_width_in_points =
      CTLineGetTypographicBounds(line, NULL, NULL, NULL) * webkit_scale_factor;
  SkPoint point = GetHeaderFooterPosition(webkit_scale_factor,
                                          page_layout, horizontal_position,
                                          vertical_position, offset_to_baseline,
                                          text_width_in_points);
  CGContextSetTextPosition(canvas, SkScalarToDouble(point.x()),
                           SkScalarToDouble(point.y()));
  CTLineDraw(line, canvas);
#endif
}

}  // namespace

// static - Not anonymous so that platform implementations can use it.
void PrintWebViewHelper::PrintHeaderAndFooter(
    WebKit::WebCanvas* canvas,
    int page_number,
    int total_pages,
    float webkit_scale_factor,
    const PageSizeMargins& page_layout,
    const DictionaryValue& header_footer_info) {
#if defined(USE_SKIA)
  skia::VectorPlatformDeviceSkia* device =
      static_cast<skia::VectorPlatformDeviceSkia*>(canvas->getTopDevice());
  device->setDrawingArea(SkPDFDevice::kMargin_DrawingArea);

  SkPaint paint;
  paint.setColor(SK_ColorBLACK);
  paint.setTextEncoding(SkPaint::kUTF16_TextEncoding);
  paint.setTextSize(SkDoubleToScalar(printing::kSettingHeaderFooterFontSize));
  paint.setTypeface(SkTypeface::CreateFromName(
      printing::kSettingHeaderFooterFontFamilyName, SkTypeface::kNormal));
#elif defined(OS_MACOSX)
  gfx::ScopedCGContextSaveGState CGContextSaveGState(canvas);
  CGContextSetCharacterSpacing(canvas,
                               printing::kSettingHeaderFooterCharacterSpacing);
  CGContextSetTextDrawingMode(canvas, kCGTextFill);
  CGContextSetGrayFillColor(canvas, kBlackGrayLevel, kOpaqueLevel);
  CGContextSelectFont(canvas, printing::kSettingHeaderFooterFontName,
                      printing::kSettingHeaderFooterFontSize,
                      kCGEncodingFontSpecific);
  ScopedCFTypeRef<CFStringRef> font_name(base::SysUTF8ToCFStringRef(
      printing::kSettingHeaderFooterFontName));
  // Flip the text (makes it appear upright as we would expect it to).
  const CGAffineTransform flip_text =  CGAffineTransformMakeScale(1.0f, -1.0f);
  ScopedCFTypeRef<CTFontRef> ct_font(CTFontCreateWithName(
      font_name,
      printing::kSettingHeaderFooterFontSize / webkit_scale_factor,
      &flip_text));
  const void* keys[] = {kCTFontAttributeName};
  const void* values[] = {ct_font};
  ScopedCFTypeRef<CFDictionaryRef> paint(CFDictionaryCreate(
      NULL, keys, values, sizeof(keys) / sizeof(keys[0]), NULL, NULL));
#endif

  // Print the headers onto the |canvas| if there is enough space to print
  // them.
  string16 date;
  string16 title;
  if (!header_footer_info.GetString(printing::kSettingHeaderFooterTitle,
                                    &title) ||
      !header_footer_info.GetString(printing::kSettingHeaderFooterDate,
                                    &date)) {
    NOTREACHED();
  }
  string16 header_text = date + title;

  // Used for height calculations. Note that the width may be undefined.
  SkRect header_vertical_bounds;
#if defined(USE_SKIA)
  paint.measureText(header_text.c_str(), header_text.length() * sizeof(char16),
                    &header_vertical_bounds, 0);
#elif defined(OS_MACOSX)
  header_vertical_bounds.fTop = CTFontGetAscent(ct_font) * webkit_scale_factor;
  header_vertical_bounds.fBottom = -CTFontGetDescent(ct_font) *
                                   webkit_scale_factor;
#endif
  double text_height = printing::kSettingHeaderFooterInterstice +
                       header_vertical_bounds.height();
  if (text_height <= page_layout.margin_top) {
    PrintHeaderFooterText(date, canvas, paint, webkit_scale_factor, page_layout,
                          printing::LEFT, printing::TOP,
                          header_vertical_bounds.top());
    PrintHeaderFooterText(title, canvas, paint, webkit_scale_factor,
                          page_layout, printing::CENTER, printing::TOP,
                          header_vertical_bounds.top());
  }

  // Prints the footers onto the |canvas| if there is enough space to print
  // them.
  string16 page_of_total_pages = base::IntToString16(page_number) +
                                 UTF8ToUTF16("/") +
                                 base::IntToString16(total_pages);
  string16 url;
  if (!header_footer_info.GetString(printing::kSettingHeaderFooterURL,
                                    &url)) {
    NOTREACHED();
  }
  string16 footer_text = page_of_total_pages + url;

  // Used for height calculations. Note that the width may be undefined.
  SkRect footer_vertical_bounds;
#if defined(USE_SKIA)
  paint.measureText(footer_text.c_str(), footer_text.length() * sizeof(char16),
                    &footer_vertical_bounds, 0);
#elif defined(OS_MACOSX)
  footer_vertical_bounds.fTop = header_vertical_bounds.fTop;
  footer_vertical_bounds.fBottom = header_vertical_bounds.fBottom;
#endif
  text_height = printing::kSettingHeaderFooterInterstice +
                footer_vertical_bounds.height();
  if (text_height <= page_layout.margin_bottom) {
    PrintHeaderFooterText(page_of_total_pages, canvas, paint,
                          webkit_scale_factor, page_layout, printing::RIGHT,
                          printing::BOTTOM, footer_vertical_bounds.bottom());
    PrintHeaderFooterText(url, canvas, paint, webkit_scale_factor, page_layout,
                          printing::LEFT, printing::BOTTOM,
                          footer_vertical_bounds.bottom());
  }

#if defined(USE_SKIA)
  device->setDrawingArea(SkPDFDevice::kContent_DrawingArea);
#endif
}

PrepareFrameAndViewForPrint::PrepareFrameAndViewForPrint(
    const PrintMsg_Print_Params& print_params,
    WebFrame* frame,
    WebNode* node)
        : frame_(frame),
          web_view_(frame->view()),
          dpi_(static_cast<int>(print_params.dpi)),
          expected_pages_count_(0),
          use_browser_overlays_(true),
          finished_(false) {
  gfx::Size canvas_size;
  CalculatePrintCanvasSize(print_params, &canvas_size);

  if (WebFrame* web_frame = web_view_->mainFrame())
    prev_scroll_offset_ = web_frame->scrollOffset();
  prev_view_size_ = web_view_->size();

  if (node)
    node_to_print_ = *node;

  StartPrinting(canvas_size);
}

PrepareFrameAndViewForPrint::~PrepareFrameAndViewForPrint() {
  FinishPrinting();
}

void PrepareFrameAndViewForPrint::UpdatePrintParams(
    const PrintMsg_Print_Params& print_params) {
  DCHECK(!finished_);
  gfx::Size canvas_size;
  CalculatePrintCanvasSize(print_params, &canvas_size);
  if (canvas_size == print_canvas_size_)
    return;

  frame_->printEnd();
  dpi_ = static_cast<int>(print_params.dpi);
  StartPrinting(canvas_size);
}

void PrepareFrameAndViewForPrint::StartPrinting(
    const gfx::Size& print_canvas_size) {
  print_canvas_size_ = print_canvas_size;

  // Layout page according to printer page size. Since WebKit shrinks the
  // size of the page automatically (from 125% to 200%) we trick it to
  // think the page is 125% larger so the size of the page is correct for
  // minimum (default) scaling.
  // This is important for sites that try to fill the page.
  gfx::Size print_layout_size(print_canvas_size_);
  print_layout_size.set_height(static_cast<int>(
      static_cast<double>(print_layout_size.height()) * 1.25));

  web_view_->resize(print_layout_size);

  expected_pages_count_ = frame_->printBegin(print_canvas_size_, node_to_print_,
                                             dpi_, &use_browser_overlays_);
}

void PrepareFrameAndViewForPrint::FinishPrinting() {
  if (!finished_) {
    finished_ = true;
    frame_->printEnd();
    web_view_->resize(prev_view_size_);
    if (WebFrame* web_frame = web_view_->mainFrame())
      web_frame->setScrollOffset(prev_scroll_offset_);
  }
}

PrintWebViewHelper::PrintWebViewHelper(RenderView* render_view)
    : RenderViewObserver(render_view),
      RenderViewObserverTracker<PrintWebViewHelper>(render_view),
      print_web_view_(NULL),
      is_preview_(switches::IsPrintPreviewEnabled()),
      user_cancelled_scripted_print_count_(0),
      notify_browser_of_print_failure_(true) {
}

PrintWebViewHelper::~PrintWebViewHelper() {}

// Prints |frame| which called window.print().
void PrintWebViewHelper::PrintPage(WebKit::WebFrame* frame) {
  DCHECK(frame);

  // Allow Prerendering to cancel this print request if necessary.
  if (prerender::PrerenderHelper::IsPrerendering(render_view())) {
    Send(new ChromeViewHostMsg_CancelPrerenderForPrinting(routing_id()));
    return;
  }

  if (IsScriptInitiatedPrintTooFrequent(frame))
    return;
  IncrementScriptedPrintCount();

  if (is_preview_) {
    print_preview_context_.InitWithFrame(frame);
    RequestPrintPreview();
  } else {
    Print(frame, NULL);
  }
}

bool PrintWebViewHelper::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PrintWebViewHelper, message)
    IPC_MESSAGE_HANDLER(PrintMsg_PrintPages, OnPrintPages)
    IPC_MESSAGE_HANDLER(PrintMsg_PrintForSystemDialog, OnPrintForSystemDialog)
    IPC_MESSAGE_HANDLER(PrintMsg_InitiatePrintPreview, OnInitiatePrintPreview)
    IPC_MESSAGE_HANDLER(PrintMsg_PrintNodeUnderContextMenu,
                        OnPrintNodeUnderContextMenu)
    IPC_MESSAGE_HANDLER(PrintMsg_PrintPreview, OnPrintPreview)
    IPC_MESSAGE_HANDLER(PrintMsg_PrintForPrintPreview, OnPrintForPrintPreview)
    IPC_MESSAGE_HANDLER(PrintMsg_PrintingDone, OnPrintingDone)
    IPC_MESSAGE_HANDLER(PrintMsg_ResetScriptedPrintCount,
                        ResetScriptedPrintCount)
    IPC_MESSAGE_HANDLER(PrintMsg_PreviewPrintingRequestCancelled,
                        DisplayPrintJobError)
    IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()
  return handled;
}

void PrintWebViewHelper::OnPrintForPrintPreview(
    const DictionaryValue& job_settings) {
  DCHECK(is_preview_);
  // If still not finished with earlier print request simply ignore.
  if (print_web_view_)
    return;

  if (!render_view()->webview())
    return;
  WebFrame* main_frame = render_view()->webview()->mainFrame();
  if (!main_frame)
    return;

  WebDocument document = main_frame->document();
  // <object> with id="pdf-viewer" is created in
  // chrome/browser/resources/print_preview/print_preview.js
  WebElement pdf_element = document.getElementById("pdf-viewer");
  if (pdf_element.isNull()) {
    NOTREACHED();
    return;
  }

  WebFrame* pdf_frame = pdf_element.document().frame();
  scoped_ptr<PrepareFrameAndViewForPrint> prepare;
  if (!InitPrintSettingsAndPrepareFrame(pdf_frame, &pdf_element, &prepare)) {
    NOTREACHED() << "Failed to initialize print page settings";
    return;
  }

  if (!UpdatePrintSettings(job_settings)) {
    DidFinishPrinting(FAIL_PRINT);
    return;
  }

  // Render Pages for printing.
  if (!RenderPagesForPrint(pdf_frame, &pdf_element, prepare.get()))
    DidFinishPrinting(FAIL_PRINT);
}

bool PrintWebViewHelper::GetPrintFrame(WebKit::WebFrame** frame) {
  DCHECK(frame);
  DCHECK(render_view()->webview());
  if (!render_view()->webview())
    return false;

  // If the user has selected text in the currently focused frame we print
  // only that frame (this makes print selection work for multiple frames).
  *frame = render_view()->webview()->focusedFrame()->hasSelection() ?
      render_view()->webview()->focusedFrame() :
      render_view()->webview()->mainFrame();
  return true;
}

void PrintWebViewHelper::OnPrintPages() {
  WebFrame* frame;
  if (GetPrintFrame(&frame))
    Print(frame, NULL);
}

void PrintWebViewHelper::OnPrintForSystemDialog() {
  WebFrame* frame = print_preview_context_.frame();
  if (!frame) {
    NOTREACHED();
    return;
  }

  WebNode* node = print_preview_context_.node();
  if (!node) {
    Print(frame, NULL);
  } else {
    // Make a copy of the node, because |print_preview_context_| will reset its
    // copy when the print preview tab closes.
    WebNode duplicate_node(*node);
    Print(frame, &duplicate_node);
  }
}

void PrintWebViewHelper::OnPrintPreview(const DictionaryValue& settings) {
  DCHECK(is_preview_);
  print_preview_context_.OnPrintPreview();

  if (!InitPrintSettings(print_preview_context_.frame(),
                         print_preview_context_.node())) {
    NOTREACHED();
    return;
  }

  if (!UpdatePrintSettings(settings)) {
    DidFinishPrinting(FAIL_PREVIEW);
    return;
  }

  if (!print_pages_params_->params.is_first_request &&
      old_print_pages_params_.get() &&
      PrintMsg_Print_Params_IsEqual(*old_print_pages_params_,
                                    *print_pages_params_)) {
    PrintHostMsg_DidPreviewDocument_Params preview_params;
    preview_params.reuse_existing_data = true;
    preview_params.data_size = 0;
    preview_params.document_cookie =
        print_pages_params_->params.document_cookie;
    preview_params.expected_pages_count =
        print_preview_context_.total_page_count();
    preview_params.modifiable = print_preview_context_.IsModifiable();
    preview_params.preview_request_id =
        print_pages_params_->params.preview_request_id;

    Send(new PrintHostMsg_PagesReadyForPreview(routing_id(), preview_params));
    return;
  }
  // Always clear |old_print_pages_params_| before rendering the pages.
  old_print_pages_params_.reset();

  // PDF printer device supports alpha blending.
  print_pages_params_->params.supports_alpha_blend = true;
  if (CreatePreviewDocument())
    DidFinishPrinting(OK);
  else
    DidFinishPrinting(FAIL_PREVIEW);
}

bool PrintWebViewHelper::CreatePreviewDocument() {
  PrintMsg_Print_Params print_params = print_pages_params_->params;
  const std::vector<int>& pages = print_pages_params_->pages;
  if (!print_preview_context_.CreatePreviewDocument(&print_params, pages))
    return false;
  PrintHostMsg_DidGetPreviewPageCount_Params params;
  params.page_count = print_preview_context_.total_page_count();
  params.is_modifiable = print_preview_context_.IsModifiable();
  params.document_cookie = print_pages_params_->params.document_cookie;
  params.preview_request_id = print_pages_params_->params.preview_request_id;
  Send(new PrintHostMsg_DidGetPreviewPageCount(routing_id(), params));
  if (CheckForCancel())
    return false;

  int page_number;
  while ((page_number = print_preview_context_.GetNextPageNumber()) >= 0) {
    if (!RenderPreviewPage(page_number))
      return false;
    if (CheckForCancel())
      return false;
  };

  // Finished generating preview. Finalize the document.
  if (!FinalizePreviewDocument())
    return false;
  print_preview_context_.Finished();
  return true;
}

bool PrintWebViewHelper::FinalizePreviewDocument() {
  print_preview_context_.FinalizePreviewDocument();

  // Get the size of the resulting metafile.
  printing::PreviewMetafile* metafile = print_preview_context_.metafile();
  uint32 buf_size = metafile->GetDataSize();
  DCHECK_GT(buf_size, 0u);

  PrintHostMsg_DidPreviewDocument_Params preview_params;
  preview_params.reuse_existing_data = false;
  preview_params.data_size = buf_size;
  preview_params.document_cookie = print_pages_params_->params.document_cookie;
  preview_params.expected_pages_count =
      print_preview_context_.total_page_count();
  preview_params.modifiable = print_preview_context_.IsModifiable();
  preview_params.preview_request_id =
      print_pages_params_->params.preview_request_id;

  // Ask the browser to create the shared memory for us.
  if (!CopyMetafileDataToSharedMem(metafile,
                                   &(preview_params.metafile_data_handle))) {
    return false;
  }
  Send(new PrintHostMsg_PagesReadyForPreview(routing_id(), preview_params));
  return true;
}

void PrintWebViewHelper::OnPrintingDone(bool success) {
  notify_browser_of_print_failure_ = false;
  DidFinishPrinting(success ? OK : FAIL_PRINT);
}

void PrintWebViewHelper::OnPrintNodeUnderContextMenu() {
  const WebNode& context_menu_node = render_view()->context_menu_node();
  if (context_menu_node.isNull()) {
    NOTREACHED();
    return;
  }

  // Make a copy of the node, in case RenderView::OnContextMenuClosed resets
  // its |context_menu_node_|.
  if (is_preview_) {
    print_preview_context_.InitWithNode(context_menu_node);
    RequestPrintPreview();
  } else {
    WebNode duplicate_node(context_menu_node);
    Print(duplicate_node.document().frame(), &duplicate_node);
  }
}

void PrintWebViewHelper::OnInitiatePrintPreview() {
  DCHECK(is_preview_);
  WebFrame* frame;
  if (GetPrintFrame(&frame)) {
    print_preview_context_.InitWithFrame(frame);
    RequestPrintPreview();
  }
}

void PrintWebViewHelper::Print(WebKit::WebFrame* frame, WebKit::WebNode* node) {
  // If still not finished with earlier print request simply ignore.
  if (print_web_view_)
    return;

  // Initialize print settings.
  scoped_ptr<PrepareFrameAndViewForPrint> prepare;
  if (!InitPrintSettingsAndPrepareFrame(frame, node, &prepare))
    return;  // Failed to init print page settings.

  int expected_page_count = 0;
  bool use_browser_overlays = true;

  expected_page_count = prepare->GetExpectedPageCount();
  if (expected_page_count)
    use_browser_overlays = prepare->ShouldUseBrowserOverlays();

  // Release the prepare before going any further, since we are going to
  // show UI and wait for the user.
  prepare.reset();

  // Some full screen plugins can say they don't want to print.
  if (!expected_page_count) {
    DidFinishPrinting(OK);  // Release resources and fail silently.
    return;
  }

  // Ask the browser to show UI to retrieve the final print settings.
  if (!GetPrintSettingsFromUser(frame, expected_page_count,
                                use_browser_overlays)) {
    DidFinishPrinting(OK);  // Release resources and fail silently.
    return;
  }

  // Render Pages for printing.
  if (!RenderPagesForPrint(frame, node, NULL))
    DidFinishPrinting(FAIL_PRINT);
  ResetScriptedPrintCount();
}

void PrintWebViewHelper::DidFinishPrinting(PrintingResult result) {
  bool store_print_pages_params = true;
  if (result == FAIL_PRINT) {
    DisplayPrintJobError();

    if (notify_browser_of_print_failure_) {
      int cookie = print_pages_params_->params.document_cookie;
      Send(new PrintHostMsg_PrintingFailed(routing_id(), cookie));
    }
  } else if (result == FAIL_PREVIEW) {
    DCHECK(is_preview_);
    store_print_pages_params = false;
    int cookie = print_pages_params_->params.document_cookie;
    if (notify_browser_of_print_failure_)
      Send(new PrintHostMsg_PrintPreviewFailed(routing_id(), cookie));
    else
      Send(new PrintHostMsg_PrintPreviewCancelled(routing_id(), cookie));
    print_preview_context_.Failed();
  }

  if (print_web_view_) {
    print_web_view_->close();
    print_web_view_ = NULL;
  }

  if (store_print_pages_params) {
    old_print_pages_params_.reset(print_pages_params_.release());
  } else {
    print_pages_params_.reset();
    old_print_pages_params_.reset();
  }

  notify_browser_of_print_failure_ = true;
}

bool PrintWebViewHelper::CopyAndPrint(WebKit::WebFrame* web_frame) {
  // Create a new WebView with the same settings as the current display one.
  // Except that we disable javascript (don't want any active content running
  // on the page).
  WebPreferences prefs = render_view()->webkit_preferences();
  prefs.javascript_enabled = false;
  prefs.java_enabled = false;

  print_web_view_ = WebView::create(this);
  prefs.Apply(print_web_view_);
  print_web_view_->initializeMainFrame(this);

  print_pages_params_->pages.clear();  // Print all pages of selection.

  std::string html = web_frame->selectionAsMarkup().utf8();
  std::string url_str = "data:text/html;charset=utf-8,";
  url_str.append(html);
  GURL url(url_str);

  // When loading is done this will call DidStopLoading that will do the
  // actual printing.
  print_web_view_->mainFrame()->loadRequest(WebURLRequest(url));

  return true;
}

#if defined(OS_MACOSX) || defined(OS_WIN)
bool PrintWebViewHelper::PrintPages(const PrintMsg_PrintPages_Params& params,
                                    WebFrame* frame,
                                    WebNode* node,
                                    PrepareFrameAndViewForPrint* prepare) {
  PrintMsg_Print_Params print_params = params.params;
  scoped_ptr<PrepareFrameAndViewForPrint> prep_frame_view;
  if (!prepare) {
    prep_frame_view.reset(new PrepareFrameAndViewForPrint(print_params, frame,
                                                          node));
    prepare = prep_frame_view.get();
  }
  UpdatePrintableSizeInPrintParameters(frame, node, prepare, &print_params);

  int page_count = prepare->GetExpectedPageCount();
  if (!page_count)
    return false;
  Send(new PrintHostMsg_DidGetPrintedPagesCount(routing_id(),
                                                print_params.document_cookie,
                                                page_count));

  const gfx::Size& canvas_size = prepare->GetPrintCanvasSize();
  PrintMsg_PrintPage_Params page_params;
  page_params.params = print_params;
  if (params.pages.empty()) {
    for (int i = 0; i < page_count; ++i) {
      page_params.page_number = i;
      PrintPageInternal(page_params, canvas_size, frame);
    }
  } else {
    for (size_t i = 0; i < params.pages.size(); ++i) {
      if (params.pages[i] >= page_count)
        break;
      page_params.page_number = params.pages[i];
      PrintPageInternal(page_params, canvas_size, frame);
    }
  }
  return true;
}
#endif  // OS_MACOSX || OS_WIN

void PrintWebViewHelper::didStopLoading() {
  PrintMsg_PrintPages_Params* params = print_pages_params_.get();
  DCHECK(params != NULL);
  PrintPages(*params, print_web_view_->mainFrame(), NULL, NULL);
}

// static - Not anonymous so that platform implementations can use it.
void PrintWebViewHelper::GetPageSizeAndMarginsInPoints(
    WebFrame* frame,
    int page_index,
    const PrintMsg_Print_Params& default_params,
    PageSizeMargins* page_layout_in_points) {
  int dpi = GetDPI(&default_params);

  WebSize page_size_in_pixels(
      ConvertUnit(default_params.page_size.width(),
                  dpi, printing::kPixelsPerInch),
      ConvertUnit(default_params.page_size.height(),
                  dpi, printing::kPixelsPerInch));
  int margin_top_in_pixels = ConvertUnit(
      default_params.margin_top,
      dpi, printing::kPixelsPerInch);
  int margin_right_in_pixels = ConvertUnit(
      default_params.page_size.width() -
      default_params.printable_size.width() - default_params.margin_left,
      dpi, printing::kPixelsPerInch);
  int margin_bottom_in_pixels = ConvertUnit(
      default_params.page_size.height() -
      default_params.printable_size.height() - default_params.margin_top,
      dpi, printing::kPixelsPerInch);
  int margin_left_in_pixels = ConvertUnit(
      default_params.margin_left,
      dpi, printing::kPixelsPerInch);

  if (frame) {
    frame->pageSizeAndMarginsInPixels(page_index,
                                      page_size_in_pixels,
                                      margin_top_in_pixels,
                                      margin_right_in_pixels,
                                      margin_bottom_in_pixels,
                                      margin_left_in_pixels);
  }

  page_layout_in_points->content_width =
      ConvertPixelsToPoint(page_size_in_pixels.width -
                           margin_left_in_pixels -
                           margin_right_in_pixels);
  page_layout_in_points->content_height =
      ConvertPixelsToPoint(page_size_in_pixels.height -
                           margin_top_in_pixels -
                           margin_bottom_in_pixels);

  // Invalid page size and/or margins. We just use the default setting.
  if (page_layout_in_points->content_width < 1.0 ||
      page_layout_in_points->content_height < 1.0) {
    GetPageSizeAndMarginsInPoints(NULL, page_index, default_params,
                                  page_layout_in_points);
    return;
  }

    page_layout_in_points->margin_top =
        ConvertPixelsToPointDouble(margin_top_in_pixels);
    page_layout_in_points->margin_right =
        ConvertPixelsToPointDouble(margin_right_in_pixels);
    page_layout_in_points->margin_bottom =
        ConvertPixelsToPointDouble(margin_bottom_in_pixels);
    page_layout_in_points->margin_left =
        ConvertPixelsToPointDouble(margin_left_in_pixels);
}

// static - Not anonymous so that platform implementations can use it.
void PrintWebViewHelper::UpdatePrintableSizeInPrintParameters(
    WebFrame* frame,
    WebNode* node,
    PrepareFrameAndViewForPrint* prepare,
    PrintMsg_Print_Params* params) {
  PageSizeMargins page_layout_in_points;
  prepare->UpdatePrintParams(*params);
  PrintWebViewHelper::GetPageSizeAndMarginsInPoints(frame, 0, *params,
                                                    &page_layout_in_points);
  int dpi = GetDPI(params);
  params->printable_size = gfx::Size(
      static_cast<int>(ConvertUnitDouble(
          page_layout_in_points.content_width,
          printing::kPointsPerInch, dpi)),
      static_cast<int>(ConvertUnitDouble(
          page_layout_in_points.content_height,
          printing::kPointsPerInch, dpi)));

  double page_width_in_points =
      page_layout_in_points.content_width +
      page_layout_in_points.margin_left +
      page_layout_in_points.margin_right;
  double page_height_in_points =
      page_layout_in_points.content_height +
      page_layout_in_points.margin_top +
      page_layout_in_points.margin_bottom;

  params->page_size = gfx::Size(
      static_cast<int>(ConvertUnitDouble(
          page_width_in_points, printing::kPointsPerInch, dpi)),
      static_cast<int>(ConvertUnitDouble(
          page_height_in_points, printing::kPointsPerInch, dpi)));

  params->margin_top = static_cast<int>(ConvertUnitDouble(
      page_layout_in_points.margin_top, printing::kPointsPerInch, dpi));
  params->margin_left = static_cast<int>(ConvertUnitDouble(
      page_layout_in_points.margin_left, printing::kPointsPerInch, dpi));
}

bool PrintWebViewHelper::InitPrintSettings(WebKit::WebFrame* frame,
                                           WebKit::WebNode* node) {
  DCHECK(frame);
  PrintMsg_PrintPages_Params settings;

  Send(new PrintHostMsg_GetDefaultPrintSettings(routing_id(),
                                                &settings.params));
  // Check if the printer returned any settings, if the settings is empty, we
  // can safely assume there are no printer drivers configured. So we safely
  // terminate.
  if (PrintMsg_Print_Params_IsEmpty(settings.params)) {
    render_view()->runModalAlertDialog(
        frame,
        l10n_util::GetStringUTF16(IDS_DEFAULT_PRINTER_NOT_FOUND_WARNING));
    return false;
  }
  if (settings.params.dpi < kMinDpi || settings.params.document_cookie == 0) {
    // Invalid print page settings.
    NOTREACHED();
    return false;
  }

  settings.pages.clear();
  print_pages_params_.reset(new PrintMsg_PrintPages_Params(settings));
  return true;
}

bool PrintWebViewHelper::InitPrintSettingsAndPrepareFrame(
    WebKit::WebFrame* frame, WebKit::WebNode* node,
    scoped_ptr<PrepareFrameAndViewForPrint>* prepare) {
  if (!InitPrintSettings(frame, node))
    return false;

  DCHECK(!prepare->get());
  prepare->reset(new PrepareFrameAndViewForPrint(print_pages_params_->params,
                                                 frame, node));
  UpdatePrintableSizeInPrintParameters(frame, node, prepare->get(),
                                       &print_pages_params_->params);
  Send(new PrintHostMsg_DidGetDocumentCookie(
        routing_id(), print_pages_params_->params.document_cookie));
  return true;
}

bool PrintWebViewHelper::UpdatePrintSettings(
    const DictionaryValue& job_settings) {
  PrintMsg_PrintPages_Params settings;

  Send(new PrintHostMsg_UpdatePrintSettings(routing_id(),
       print_pages_params_->params.document_cookie, job_settings, &settings));

  if (settings.params.dpi < kMinDpi || !settings.params.document_cookie)
    return false;

  if (!job_settings.GetString(printing::kPreviewUIAddr,
                              &(settings.params.preview_ui_addr)) ||
      !job_settings.GetInteger(printing::kPreviewRequestID,
                               &(settings.params.preview_request_id)) ||
      !job_settings.GetBoolean(printing::kIsFirstRequest,
                               &(settings.params.is_first_request))) {
    NOTREACHED();
    return false;
  }

  print_pages_params_.reset(new PrintMsg_PrintPages_Params(settings));

  if (print_pages_params_->params.display_header_footer) {
    header_footer_info_.reset(new DictionaryValue());
    header_footer_info_->SetString(printing::kSettingHeaderFooterDate,
                                   print_pages_params_->params.date);
    header_footer_info_->SetString(printing::kSettingHeaderFooterURL,
                                   print_pages_params_->params.url);
    header_footer_info_->SetString(printing::kSettingHeaderFooterTitle,
                                   print_pages_params_->params.title);
  }

  Send(new PrintHostMsg_DidGetDocumentCookie(routing_id(),
                                             settings.params.document_cookie));
  return true;
}

bool PrintWebViewHelper::GetPrintSettingsFromUser(WebKit::WebFrame* frame,
                                                  int expected_pages_count,
                                                  bool use_browser_overlays) {
  PrintHostMsg_ScriptedPrint_Params params;
  PrintMsg_PrintPages_Params print_settings;

  // The routing id is sent across as it is needed to look up the
  // corresponding RenderViewHost instance to signal and reset the
  // pump messages event.
  params.routing_id = render_view()->routing_id();
  // host_window_ may be NULL at this point if the current window is a
  // popup and the print() command has been issued from the parent. The
  // receiver of this message has to deal with this.
  params.host_window_id = render_view()->host_window();
  params.cookie = print_pages_params_->params.document_cookie;
  params.has_selection = frame->hasSelection();
  params.expected_pages_count = expected_pages_count;
  params.use_overlays = use_browser_overlays;

  Send(new PrintHostMsg_DidShowPrintDialog(routing_id()));

  print_pages_params_.reset();
  IPC::SyncMessage* msg =
      new PrintHostMsg_ScriptedPrint(routing_id(), params, &print_settings);
  msg->EnableMessagePumping();
  Send(msg);
  print_pages_params_.reset(new PrintMsg_PrintPages_Params(print_settings));
  return (print_settings.params.dpi && print_settings.params.document_cookie);
}

bool PrintWebViewHelper::RenderPagesForPrint(
    WebKit::WebFrame* frame,
    WebKit::WebNode* node,
    PrepareFrameAndViewForPrint* prepare) {
  PrintMsg_PrintPages_Params print_settings = *print_pages_params_;
  if (print_settings.params.selection_only) {
    return CopyAndPrint(frame);
  } else {
    // TODO: Always copy before printing.
    return PrintPages(print_settings, frame, node, prepare);
  }
}

#if defined(OS_POSIX)
bool PrintWebViewHelper::CopyMetafileDataToSharedMem(
    printing::Metafile* metafile,
    base::SharedMemoryHandle* shared_mem_handle) {
  uint32 buf_size = metafile->GetDataSize();
  base::SharedMemoryHandle mem_handle;
  Send(new ViewHostMsg_AllocateSharedMemoryBuffer(buf_size, &mem_handle));
  if (base::SharedMemory::IsHandleValid(mem_handle)) {
    base::SharedMemory shared_buf(mem_handle, false);
    if (shared_buf.Map(buf_size)) {
      metafile->GetData(shared_buf.memory(), buf_size);
      shared_buf.GiveToProcess(base::GetCurrentProcessHandle(),
                               shared_mem_handle);
      return true;
    }
  }
  NOTREACHED();
  return false;
}
#endif  // defined(OS_POSIX)

bool PrintWebViewHelper::IsScriptInitiatedPrintTooFrequent(
    WebKit::WebFrame* frame) {
  const int kMinSecondsToIgnoreJavascriptInitiatedPrint = 2;
  const int kMaxSecondsToIgnoreJavascriptInitiatedPrint = 32;
  bool too_frequent = false;

  // Check if there is script repeatedly trying to print and ignore it if too
  // frequent.  The first 3 times, we use a constant wait time, but if this
  // gets excessive, we switch to exponential wait time. So for a page that
  // calls print() in a loop the user will need to cancel the print dialog
  // after: [2, 2, 2, 4, 8, 16, 32, 32, ...] seconds.
  // This gives the user time to navigate from the page.
  if (user_cancelled_scripted_print_count_ > 0) {
    base::TimeDelta diff = base::Time::Now() - last_cancelled_script_print_;
    int min_wait_seconds = kMinSecondsToIgnoreJavascriptInitiatedPrint;
    if (user_cancelled_scripted_print_count_ > 3) {
      min_wait_seconds = std::min(
          kMinSecondsToIgnoreJavascriptInitiatedPrint <<
              (user_cancelled_scripted_print_count_ - 3),
          kMaxSecondsToIgnoreJavascriptInitiatedPrint);
    }
    if (diff.InSeconds() < min_wait_seconds) {
      too_frequent = true;
    }
  }

  if (!too_frequent)
    return false;

  WebString message(WebString::fromUTF8(
      "Ignoring too frequent calls to print()."));
  frame->addMessageToConsole(WebConsoleMessage(WebConsoleMessage::LevelWarning,
                                               message));
  return true;
}

void PrintWebViewHelper::ResetScriptedPrintCount() {
  // Reset cancel counter on successful print.
  user_cancelled_scripted_print_count_ = 0;
}

void PrintWebViewHelper::IncrementScriptedPrintCount() {
  ++user_cancelled_scripted_print_count_;
  last_cancelled_script_print_ = base::Time::Now();
}

void PrintWebViewHelper::DisplayPrintJobError() {
  WebView* web_view = print_web_view_;
  if (!web_view)
    web_view = render_view()->webview();

  render_view()->runModalAlertDialog(
      web_view->mainFrame(),
      l10n_util::GetStringUTF16(IDS_PRINT_SPOOL_FAILED_ERROR_TEXT));
}

void PrintWebViewHelper::RequestPrintPreview() {
  old_print_pages_params_.reset();
  Send(new PrintHostMsg_RequestPrintPreview(routing_id()));
}

bool PrintWebViewHelper::CheckForCancel() {
  bool cancel = false;
  Send(new PrintHostMsg_CheckForCancel(
      routing_id(),
      print_pages_params_->params.preview_ui_addr,
      print_pages_params_->params.preview_request_id,
      &cancel));
  if (cancel)
    notify_browser_of_print_failure_ = false;
  return cancel;
}

bool PrintWebViewHelper::PreviewPageRendered(int page_number,
                                             printing::Metafile* metafile) {
  DCHECK_GE(page_number, printing::FIRST_PAGE_INDEX);

  // For non-modifiable files, |metafile| should be NULL, so do not bother
  // sending a message.
  if (!print_preview_context_.IsModifiable()) {
    DCHECK(!metafile);
    return true;
  }

  if (!metafile) {
    NOTREACHED();
    return false;
  }

  PrintHostMsg_DidPreviewPage_Params preview_page_params;
  // Get the size of the resulting metafile.
  uint32 buf_size = metafile->GetDataSize();
  DCHECK_GT(buf_size, 0u);
  if (!CopyMetafileDataToSharedMem(
      metafile, &(preview_page_params.metafile_data_handle))) {
    return false;
  }
  preview_page_params.data_size = buf_size;
  preview_page_params.page_number = page_number;
  preview_page_params.preview_request_id =
      print_pages_params_->params.preview_request_id;

  Send(new PrintHostMsg_DidPreviewPage(routing_id(), preview_page_params));
  return true;
}

PrintWebViewHelper::PrintPreviewContext::PrintPreviewContext()
    : frame_(NULL),
      total_page_count_(0),
      actual_page_count_(0),
      current_page_index_(0),
      state_(UNINITIALIZED) {
}

PrintWebViewHelper::PrintPreviewContext::~PrintPreviewContext() {
}

void PrintWebViewHelper::PrintPreviewContext::InitWithFrame(
    WebKit::WebFrame* web_frame) {
  DCHECK(web_frame);
  state_ = INITIALIZED;
  frame_ = web_frame;
  node_.reset();
}

void PrintWebViewHelper::PrintPreviewContext::InitWithNode(
    const WebKit::WebNode& web_node) {
  DCHECK(!web_node.isNull());
  state_ = INITIALIZED;
  frame_ = web_node.document().frame();
  node_.reset(new WebNode(web_node));
}

void PrintWebViewHelper::PrintPreviewContext::OnPrintPreview() {
  DCHECK(IsReadyToRender());
  ClearContext();
}

bool PrintWebViewHelper::PrintPreviewContext::CreatePreviewDocument(
    PrintMsg_Print_Params* print_params,
    const std::vector<int>& pages) {
  DCHECK(IsReadyToRender());
  state_ = RENDERING;

  print_params_.reset(new PrintMsg_Print_Params(*print_params));

  metafile_.reset(new printing::PreviewMetafile);
  if (!metafile_->Init())
    return false;

  // Need to make sure old object gets destroyed first.
  prep_frame_view_.reset(new PrepareFrameAndViewForPrint(*print_params, frame(),
                                                         node()));
  UpdatePrintableSizeInPrintParameters(frame_, node_.get(),
                                       prep_frame_view_.get(), print_params);

  total_page_count_ = prep_frame_view_->GetExpectedPageCount();
  if (total_page_count_ == 0)
    return false;

  current_page_index_ = 0;
  if (pages.empty()) {
    actual_page_count_ = total_page_count_;
    for (int i = 0; i < actual_page_count_; ++i)
      pages_to_render_.push_back(i);
  } else {
    actual_page_count_ = pages.size();
    for (int i = 0; i < actual_page_count_; ++i) {
      int page_number = pages[i];
      if (page_number < printing::FIRST_PAGE_INDEX ||
          page_number >= total_page_count_) {
        return false;
      }
      pages_to_render_.push_back(page_number);
    }
  }

  document_render_time_ = base::TimeDelta();
  begin_time_ = base::TimeTicks::Now();

  return true;
}

void PrintWebViewHelper::PrintPreviewContext::RenderedPreviewPage(
    const base::TimeDelta& page_time) {
  DCHECK_EQ(RENDERING, state_);
  document_render_time_ += page_time;
  UMA_HISTOGRAM_TIMES("PrintPreview.RenderPDFPageTime", page_time);
}

void PrintWebViewHelper::PrintPreviewContext::FinalizePreviewDocument() {
  DCHECK_EQ(RENDERING, state_);
  state_ = DONE;

  base::TimeTicks begin_time = base::TimeTicks::Now();

  prep_frame_view_->FinishPrinting();
  metafile_->FinishDocument();

  if (actual_page_count_ <= 0) {
    NOTREACHED();
    return;
  }

  UMA_HISTOGRAM_MEDIUM_TIMES("PrintPreview.RenderToPDFTime",
                             document_render_time_);
  base::TimeDelta total_time = (base::TimeTicks::Now() - begin_time) +
                               document_render_time_;
  UMA_HISTOGRAM_MEDIUM_TIMES("PrintPreview.RenderAndGeneratePDFTime",
                             total_time);
  UMA_HISTOGRAM_MEDIUM_TIMES("PrintPreview.RenderAndGeneratePDFTimeAvgPerPage",
                             total_time / actual_page_count_);
}

void PrintWebViewHelper::PrintPreviewContext::Finished() {
  DCHECK_EQ(DONE, state_);
  ClearContext();
}

void PrintWebViewHelper::PrintPreviewContext::Failed() {
  DCHECK(state_ == INITIALIZED || state_ == RENDERING);
  state_ = INITIALIZED;
  ClearContext();
}

int PrintWebViewHelper::PrintPreviewContext::GetNextPageNumber() {
  DCHECK_EQ(RENDERING, state_);
  if (current_page_index_ >= actual_page_count_)
    return -1;
  return pages_to_render_[current_page_index_++];
}

bool PrintWebViewHelper::PrintPreviewContext::IsReadyToRender() const {
  return state_ != UNINITIALIZED;
}

bool PrintWebViewHelper::PrintPreviewContext::IsModifiable() const {
  // TODO(vandebo) I think this should only return false if the content is a
  // PDF, just because we are printing a particular node does not mean it's
  // a PDF (right?), we should check the mime type of the node.
  if (node())
    return false;
  std::string mime(frame()->dataSource()->response().mimeType().utf8());
  return mime != "application/pdf";
}

WebKit::WebFrame* PrintWebViewHelper::PrintPreviewContext::frame() const {
  return frame_;
}

WebKit::WebNode* PrintWebViewHelper::PrintPreviewContext::node() const {
  return node_.get();
}

int PrintWebViewHelper::PrintPreviewContext::total_page_count() const {
  DCHECK(IsReadyToRender());
  return total_page_count_;
}

printing::PreviewMetafile*
PrintWebViewHelper::PrintPreviewContext::metafile() const {
  return metafile_.get();
}

const PrintMsg_Print_Params&
PrintWebViewHelper::PrintPreviewContext::print_params() const {
  return *print_params_;
}

const gfx::Size&
PrintWebViewHelper::PrintPreviewContext::GetPrintCanvasSize() const {
  return prep_frame_view_->GetPrintCanvasSize();
}

void PrintWebViewHelper::PrintPreviewContext::ClearContext() {
  prep_frame_view_.reset();
  metafile_.reset();
  pages_to_render_.clear();
}
