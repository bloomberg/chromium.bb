// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/print_web_view_helper.h"

#include <string>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/process_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/print_messages.h"
#include "chrome/common/render_messages.h"
#include "chrome/renderer/prerender/prerender_helper.h"
#include "content/renderer/render_view.h"
#include "grit/generated_resources.h"
#include "printing/metafile.h"
#include "printing/units.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebConsoleMessage.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDataSource.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebNode.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURLResponse.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSize.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURLRequest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_POSIX)
#include "content/common/view_messages.h"
#endif

using printing::ConvertPixelsToPoint;
using printing::ConvertPixelsToPointDouble;
using printing::ConvertUnit;
using printing::ConvertUnitDouble;
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

}  // namespace

PrepareFrameAndViewForPrint::PrepareFrameAndViewForPrint(
    const PrintMsg_Print_Params& print_params,
    WebFrame* frame,
    WebNode* node,
    WebView* web_view)
        : frame_(frame), web_view_(web_view), expected_pages_count_(0),
          use_browser_overlays_(true) {
  int dpi = GetDPI(&print_params);
  print_canvas_size_.set_width(
      ConvertUnit(print_params.printable_size.width(), dpi,
                  print_params.desired_dpi));

  print_canvas_size_.set_height(
      ConvertUnit(print_params.printable_size.height(), dpi,
                  print_params.desired_dpi));

  // Layout page according to printer page size. Since WebKit shrinks the
  // size of the page automatically (from 125% to 200%) we trick it to
  // think the page is 125% larger so the size of the page is correct for
  // minimum (default) scaling.
  // This is important for sites that try to fill the page.
  gfx::Size print_layout_size(print_canvas_size_);
  print_layout_size.set_height(static_cast<int>(
      static_cast<double>(print_layout_size.height()) * 1.25));

  if (WebFrame* web_frame = web_view_->mainFrame())
    prev_scroll_offset_ = web_frame->scrollOffset();
  prev_view_size_ = web_view->size();

  web_view->resize(print_layout_size);

  WebNode node_to_print;
  if (node)
    node_to_print = *node;
  expected_pages_count_ = frame->printBegin(
      print_canvas_size_, node_to_print, static_cast<int>(print_params.dpi),
      &use_browser_overlays_);
}

PrepareFrameAndViewForPrint::~PrepareFrameAndViewForPrint() {
  frame_->printEnd();
  web_view_->resize(prev_view_size_);
  if (WebFrame* web_frame = web_view_->mainFrame())
    web_frame->setScrollOffset(prev_scroll_offset_);
}


PrintWebViewHelper::PrintWebViewHelper(RenderView* render_view)
    : RenderViewObserver(render_view),
      RenderViewObserverTracker<PrintWebViewHelper>(render_view),
      print_web_view_(NULL),
      script_initiated_preview_frame_(NULL),
      context_menu_preview_node_(NULL),
      user_cancelled_scripted_print_count_(0),
      notify_browser_of_print_failure_(true) {
  is_preview_ = switches::IsPrintPreviewEnabled();
}

PrintWebViewHelper::~PrintWebViewHelper() {}

// Prints |frame| which called window.print().
void PrintWebViewHelper::PrintPage(WebKit::WebFrame* frame) {
  DCHECK(frame);

  // Allow Prerendering to cancel this print request if necessary.
  if (prerender::PrerenderHelper::IsPrerendering(render_view())) {
    Send(new ViewHostMsg_CancelPrerenderForPrinting(routing_id()));
    return;
  }

  if (IsScriptInitiatedPrintTooFrequent(frame))
    return;
  IncrementScriptedPrintCount();

  if (is_preview_) {
    script_initiated_preview_frame_ = frame;
    Send(new PrintHostMsg_ScriptInitiatedPrintPreview(routing_id()));
  } else {
    Print(frame, NULL);
  }
}

bool PrintWebViewHelper::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PrintWebViewHelper, message)
    IPC_MESSAGE_HANDLER(PrintMsg_PrintForPrintPreview,
                        OnPrintForPrintPreview)
    IPC_MESSAGE_HANDLER(PrintMsg_PrintPages, OnPrintPages)
    IPC_MESSAGE_HANDLER(PrintMsg_PrintingDone, OnPrintingDone)
    IPC_MESSAGE_HANDLER(PrintMsg_PrintPreview, OnPrintPreview)
    IPC_MESSAGE_HANDLER(PrintMsg_PrintNodeUnderContextMenu,
                        OnPrintNodeUnderContextMenu)
    IPC_MESSAGE_HANDLER(PrintMsg_ResetScriptedPrintCount,
                        ResetScriptedPrintCount)
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
  // chrome/browser/resources/print_preview.js
  WebElement pdf_element = document.getElementById("pdf-viewer");
  if (pdf_element.isNull()) {
    NOTREACHED();
    return;
  }

  WebFrame* pdf_frame = pdf_element.document().frame();
  if (!InitPrintSettings(pdf_frame, &pdf_element)) {
    NOTREACHED() << "Failed to initialize print page settings";
    return;
  }

  if (!UpdatePrintSettings(job_settings)) {
    DidFinishPrinting(FAIL_PRINT);
    return;
  }

  // Render Pages for printing.
  if (!RenderPagesForPrint(pdf_frame, &pdf_element))
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

void PrintWebViewHelper::OnPrintPreview(const DictionaryValue& settings) {
  DCHECK(is_preview_);

  if (script_initiated_preview_frame_) {
    // Script initiated print preview.
    DCHECK(!context_menu_preview_node_.get());
    PrintPreview(script_initiated_preview_frame_, NULL, settings);
    script_initiated_preview_frame_ = NULL;
  } else if (context_menu_preview_node_.get()) {
    // User initiated - print node under context menu.
    DCHECK(!script_initiated_preview_frame_);
    PrintPreview(context_menu_preview_node_->document().frame(),
                 context_menu_preview_node_.get(), settings);
    context_menu_preview_node_.reset();
  } else {
    // User initiated - normal print preview.
    WebFrame* frame;
    if (GetPrintFrame(&frame))
      PrintPreview(frame, NULL, settings);
  }
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
    context_menu_preview_node_.reset(new WebNode(context_menu_node));
    Send(new PrintHostMsg_PrintPreviewNodeUnderContextMenu(routing_id()));
  } else {
    WebNode duplicate_node(context_menu_node);
    Print(duplicate_node.document().frame(), &duplicate_node);
  }
}

void PrintWebViewHelper::Print(WebKit::WebFrame* frame, WebKit::WebNode* node) {
  // If still not finished with earlier print request simply ignore.
  if (print_web_view_)
    return;

  // Initialize print settings.
  if (!InitPrintSettings(frame, node))
    return;  // Failed to init print page settings.

  int expected_pages_count = 0;
  bool use_browser_overlays = true;

  // Prepare once to calculate the estimated page count.  This must be in
  // a scope for itself (see comments on PrepareFrameAndViewForPrint).
  {
    PrepareFrameAndViewForPrint prep_frame_view(
        print_pages_params_->params, frame, node, frame->view());
    expected_pages_count = prep_frame_view.GetExpectedPageCount();
    if (expected_pages_count)
      use_browser_overlays = prep_frame_view.ShouldUseBrowserOverlays();
  }

  // Some full screen plugins can say they don't want to print.
  if (!expected_pages_count) {
    DidFinishPrinting(OK);  // Release resources and fail silently.
    return;
  }

  // Ask the browser to show UI to retrieve the final print settings.
  if (!GetPrintSettingsFromUser(frame, expected_pages_count,
                                use_browser_overlays)) {
    DidFinishPrinting(OK);  // Release resources and fail silently.
    return;
  }

  // Render Pages for printing.
  if (!RenderPagesForPrint(frame, node))
    DidFinishPrinting(FAIL_PRINT);
  ResetScriptedPrintCount();
}

void PrintWebViewHelper::PrintPreview(WebKit::WebFrame* frame,
                                      WebKit::WebNode* node,
                                      const DictionaryValue& settings) {
  DCHECK(is_preview_);

  if (!InitPrintSettings(frame, node)) {
    NOTREACHED() << "Failed to initialize print page settings";
    return;
  }

  if (!UpdatePrintSettings(settings)) {
    DidFinishPrinting(FAIL_PREVIEW);
    return;
  }

  // Render Pages for printing.
  if (!RenderPagesForPreview(frame, node))
    DidFinishPrinting(FAIL_PREVIEW);
}

void PrintWebViewHelper::DidFinishPrinting(PrintingResult result) {
  if (result == FAIL_PRINT) {
    WebView* web_view = print_web_view_;
    if (!web_view)
      web_view = render_view()->webview();

    render_view()->runModalAlertDialog(
        web_view->mainFrame(),
        l10n_util::GetStringUTF16(IDS_PRINT_SPOOL_FAILED_ERROR_TEXT));

    if (notify_browser_of_print_failure_) {
      int cookie = print_pages_params_->params.document_cookie;
      Send(new PrintHostMsg_PrintingFailed(routing_id(), cookie));
    }
  } else if (result == FAIL_PREVIEW) {
    int cookie = print_pages_params_->params.document_cookie;
    Send(new PrintHostMsg_PrintPreviewFailed(routing_id(), cookie));
  }

  if (print_web_view_) {
    print_web_view_->close();
    print_web_view_ = NULL;
  }
  print_pages_params_.reset();
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
                                    WebNode* node) {
  PrintMsg_Print_Params printParams = params.params;
  UpdatePrintableSizeInPrintParameters(frame, node, &printParams);

  PrepareFrameAndViewForPrint prep_frame_view(printParams,
                                              frame,
                                              node,
                                              frame->view());
  int page_count = prep_frame_view.GetExpectedPageCount();

  Send(new PrintHostMsg_DidGetPrintedPagesCount(routing_id(),
                                                printParams.document_cookie,
                                                page_count));
  if (!page_count)
    return false;

  const gfx::Size& canvas_size = prep_frame_view.GetPrintCanvasSize();
  PrintMsg_PrintPage_Params page_params;
  page_params.params = printParams;
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
  PrintPages(*params, print_web_view_->mainFrame(), NULL);
}

void PrintWebViewHelper::GetPageSizeAndMarginsInPoints(
    WebFrame* frame,
    int page_index,
    const PrintMsg_Print_Params& default_params,
    double* content_width_in_points,
    double* content_height_in_points,
    double* margin_top_in_points,
    double* margin_right_in_points,
    double* margin_bottom_in_points,
    double* margin_left_in_points) {
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

  *content_width_in_points = ConvertPixelsToPoint(page_size_in_pixels.width -
                                                  margin_left_in_pixels -
                                                  margin_right_in_pixels);
  *content_height_in_points = ConvertPixelsToPoint(page_size_in_pixels.height -
                                                   margin_top_in_pixels -
                                                   margin_bottom_in_pixels);

  // Invalid page size and/or margins. We just use the default setting.
  if (*content_width_in_points < 1.0 || *content_height_in_points < 1.0) {
    GetPageSizeAndMarginsInPoints(NULL,
                                  page_index,
                                  default_params,
                                  content_width_in_points,
                                  content_height_in_points,
                                  margin_top_in_points,
                                  margin_right_in_points,
                                  margin_bottom_in_points,
                                  margin_left_in_points);
    return;
  }

  if (margin_top_in_points)
    *margin_top_in_points =
        ConvertPixelsToPointDouble(margin_top_in_pixels);
  if (margin_right_in_points)
    *margin_right_in_points =
        ConvertPixelsToPointDouble(margin_right_in_pixels);
  if (margin_bottom_in_points)
    *margin_bottom_in_points =
        ConvertPixelsToPointDouble(margin_bottom_in_pixels);
  if (margin_left_in_points)
    *margin_left_in_points =
        ConvertPixelsToPointDouble(margin_left_in_pixels);
}

bool PrintWebViewHelper::IsModifiable(WebKit::WebFrame* frame,
                                      WebKit::WebNode* node) {
  if (node)
    return false;
  std::string mime(frame->dataSource()->response().mimeType().utf8());
  if (mime == "application/pdf")
    return false;
  return true;
}

void PrintWebViewHelper::UpdatePrintableSizeInPrintParameters(
    WebFrame* frame,
    WebNode* node,
    PrintMsg_Print_Params* params) {
  double content_width_in_points;
  double content_height_in_points;
  double margin_top_in_points;
  double margin_right_in_points;
  double margin_bottom_in_points;
  double margin_left_in_points;
  PrepareFrameAndViewForPrint prepare(*params, frame, node, frame->view());
  PrintWebViewHelper::GetPageSizeAndMarginsInPoints(frame, 0, *params,
      &content_width_in_points, &content_height_in_points,
      &margin_top_in_points, &margin_right_in_points,
      &margin_bottom_in_points, &margin_left_in_points);
  int dpi = GetDPI(params);
  params->printable_size = gfx::Size(
      static_cast<int>(ConvertUnitDouble(content_width_in_points,
          printing::kPointsPerInch, dpi)),
      static_cast<int>(ConvertUnitDouble(content_height_in_points,
          printing::kPointsPerInch, dpi)));

  double page_width_in_points = content_width_in_points +
      margin_left_in_points + margin_right_in_points;
  double page_height_in_points = content_height_in_points +
      margin_top_in_points + margin_bottom_in_points;

  params->page_size = gfx::Size(
      static_cast<int>(ConvertUnitDouble(
          page_width_in_points, printing::kPointsPerInch, dpi)),
      static_cast<int>(ConvertUnitDouble(
          page_height_in_points, printing::kPointsPerInch, dpi)));

  params->margin_top = static_cast<int>(ConvertUnitDouble(
      margin_top_in_points, printing::kPointsPerInch, dpi));
  params->margin_left = static_cast<int>(ConvertUnitDouble(
      margin_left_in_points, printing::kPointsPerInch, dpi));
}

bool PrintWebViewHelper::InitPrintSettings(WebKit::WebFrame* frame,
                                           WebKit::WebNode* node) {
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
  if (!settings.params.dpi || !settings.params.document_cookie) {
    // Invalid print page settings.
    NOTREACHED();
    return false;
  }
  UpdatePrintableSizeInPrintParameters(frame, node, &settings.params);
  settings.pages.clear();
  print_pages_params_.reset(new PrintMsg_PrintPages_Params(settings));
  return true;
}

bool PrintWebViewHelper::UpdatePrintSettings(
    const DictionaryValue& job_settings) {
  PrintMsg_PrintPages_Params settings;
  Send(new PrintHostMsg_UpdatePrintSettings(routing_id(),
      print_pages_params_->params.document_cookie, job_settings, &settings));

  if (!settings.params.dpi)
    return false;

  print_pages_params_.reset(new PrintMsg_PrintPages_Params(settings));
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

  print_pages_params_.reset();
  IPC::SyncMessage* msg =
      new PrintHostMsg_ScriptedPrint(routing_id(), params, &print_settings);
  msg->EnableMessagePumping();
  Send(msg);
  print_pages_params_.reset(new PrintMsg_PrintPages_Params(print_settings));
  return (print_settings.params.dpi && print_settings.params.document_cookie);
}

bool PrintWebViewHelper::RenderPagesForPrint(WebKit::WebFrame* frame,
                                             WebKit::WebNode* node) {
  PrintMsg_PrintPages_Params print_settings = *print_pages_params_;
  if (print_settings.params.selection_only) {
    return CopyAndPrint(frame);
  } else {
    // TODO: Always copy before printing.
    return PrintPages(print_settings, frame, node);
  }
}

bool PrintWebViewHelper::RenderPagesForPreview(WebKit::WebFrame* frame,
                                               WebKit::WebNode* node) {
  PrintMsg_PrintPages_Params print_settings = *print_pages_params_;
  // PDF printer device supports alpha blending.
  print_settings.params.supports_alpha_blend = true;
  // TODO(kmadhusu): Handle print selection.
  return CreatePreviewDocument(print_settings, frame, node);
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
      WebString message(WebString::fromUTF8(
          "Ignoring too frequent calls to print()."));
      frame->addMessageToConsole(WebConsoleMessage(
          WebConsoleMessage::LevelWarning,
          message));
      return true;
    }
  }
  return false;
}

void PrintWebViewHelper::ResetScriptedPrintCount() {
  // Reset cancel counter on successful print.
  user_cancelled_scripted_print_count_ = 0;
}

void PrintWebViewHelper::IncrementScriptedPrintCount() {
  ++user_cancelled_scripted_print_count_;
  last_cancelled_script_print_ = base::Time::Now();
}
