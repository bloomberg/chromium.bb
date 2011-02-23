// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/print_web_view_helper.h"

#include <string>

#include "base/logging.h"
#include "base/process_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/render_messages_params.h"
#include "chrome/renderer/render_view.h"
#include "grit/generated_resources.h"
#include "printing/units.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebConsoleMessage.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebNode.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebRect.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebScreenInfo.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSize.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURLRequest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "ui/base/l10n/l10n_util.h"
#include "webkit/glue/webkit_glue.h"

using printing::ConvertPixelsToPoint;
using printing::ConvertPixelsToPointDouble;
using printing::ConvertUnit;
using printing::ConvertUnitDouble;
using WebKit::WebConsoleMessage;
using WebKit::WebDocument;
using WebKit::WebElement;
using WebKit::WebFrame;
using WebKit::WebNode;
using WebKit::WebRect;
using WebKit::WebSize;
using WebKit::WebScreenInfo;
using WebKit::WebString;
using WebKit::WebURLRequest;
using WebKit::WebView;

namespace {

int GetDPI(const ViewMsg_Print_Params* print_params) {
#if defined(OS_MACOSX)
  // On the Mac, the printable area is in points, don't do any scaling based
  // on dpi.
  return printing::kPointsPerInch;
#else
  return static_cast<int>(print_params->dpi);
#endif  // defined(OS_MACOSX)
}

}  // namespace

PrepareFrameAndViewForPrint::PrepareFrameAndViewForPrint(
    const ViewMsg_Print_Params& print_params,
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
      print_web_view_(NULL),
      user_cancelled_scripted_print_count_(0),
      is_preview_(false) {}

PrintWebViewHelper::~PrintWebViewHelper() {}

void PrintWebViewHelper::PrintFrame(WebFrame* frame,
                                    bool script_initiated,
                                    bool is_preview) {
  Print(frame, NULL, script_initiated, is_preview);
}

void PrintWebViewHelper::PrintNode(WebNode* node,
                                   bool script_initiated,
                                   bool is_preview) {
  Print(node->document().frame(), node, script_initiated, is_preview);
}

bool PrintWebViewHelper::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PrintWebViewHelper, message)
    IPC_MESSAGE_HANDLER(ViewMsg_PrintForPrintPreview,
                        OnPrintForPrintPreview)
    IPC_MESSAGE_HANDLER(ViewMsg_PrintPages, OnPrintPages)
    IPC_MESSAGE_HANDLER(ViewMsg_PrintingDone, OnPrintingDone)
    IPC_MESSAGE_HANDLER(ViewMsg_PrintPreview, OnPrintPreview)
    IPC_MESSAGE_HANDLER(ViewMsg_PrintNodeUnderContextMenu,
                        OnPrintNodeUnderContextMenu)
    IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()
  return handled;
}

void PrintWebViewHelper::OnPrintForPrintPreview() {
  if (!render_view()->webview())
    return;
  WebFrame* main_frame = render_view()->webview()->mainFrame();
  if (!main_frame)
    return;

  WebDocument document = main_frame->document();
  // <object> with id="pdf-viewer" is created in
  // chrome/browser/resources/print_preview.js
  WebElement element = document.getElementById("pdf-viewer");
  if (element.isNull()) {
    NOTREACHED();
    return;
  }

  PrintNode(&element, false, false);
}

void PrintWebViewHelper::OnPrint(bool is_preview) {
  DCHECK(render_view()->webview());
  if (!render_view()->webview())
    return;

  // If the user has selected text in the currently focused frame we print
  // only that frame (this makes print selection work for multiple frames).
  if (render_view()->webview()->focusedFrame()->hasSelection())
    PrintFrame(render_view()->webview()->focusedFrame(), false, is_preview);
  else
    PrintFrame(render_view()->webview()->mainFrame(), false, is_preview);
}

void PrintWebViewHelper::OnPrintPages() {
  OnPrint(false);
}

void PrintWebViewHelper::OnPrintingDone(int document_cookie, bool success) {
  // Ignoring document cookie here since only one print job can be outstanding
  // per renderer and document_cookie is 0 when printing is successful.
  DidFinishPrinting(success);
}

void PrintWebViewHelper::OnPrintPreview() {
  OnPrint(true);
}

void PrintWebViewHelper::OnPrintNodeUnderContextMenu() {
  if (render_view()->context_menu_node().isNull()) {
    NOTREACHED();
    return;
  }

  // Make a copy of the node, since we will do a sync call to the browser and
  // during that time OnContextMenuClosed might reset context_menu_node_.
  WebNode context_menu_node(render_view()->context_menu_node());
  PrintNode(&context_menu_node, false, false);
}

void PrintWebViewHelper::Print(WebKit::WebFrame* frame,
                               WebNode* node,
                               bool script_initiated,
                               bool is_preview) {
  const int kMinSecondsToIgnoreJavascriptInitiatedPrint = 2;
  const int kMaxSecondsToIgnoreJavascriptInitiatedPrint = 2 * 60;  // 2 Minutes.

  // If still not finished with earlier print request simply ignore.
  if (print_web_view_)
    return;

  // Check if there is script repeatedly trying to print and ignore it if too
  // frequent.  We use exponential wait time so for a page that calls print() in
  // a loop the user will need to cancel the print dialog after 2 seconds, 4
  // seconds, 8, ... up to the maximum of 2 minutes.
  // This gives the user time to navigate from the page.
  if (script_initiated && (user_cancelled_scripted_print_count_ > 0)) {
    base::TimeDelta diff = base::Time::Now() - last_cancelled_script_print_;
    int min_wait_seconds = std::min(
        kMinSecondsToIgnoreJavascriptInitiatedPrint <<
            (user_cancelled_scripted_print_count_ - 1),
        kMaxSecondsToIgnoreJavascriptInitiatedPrint);
    if (diff.InSeconds() < min_wait_seconds) {
      WebString message(WebString::fromUTF8(
          "Ignoring too frequent calls to print()."));
      frame->addMessageToConsole(WebConsoleMessage(
          WebConsoleMessage::LevelWarning,
          message));
      return;
    }
  }

  bool print_cancelled = false;
  is_preview_ = is_preview;

  // Initialize print settings.
  if (!InitPrintSettings(frame, node))
    return;  // Failed to init print page settings.

  int expected_pages_count = 0;
  bool use_browser_overlays = true;

  // Prepare once to calculate the estimated page count.  This must be in
  // a scope for itself (see comments on PrepareFrameAndViewForPrint).
  {
    PrepareFrameAndViewForPrint prep_frame_view(
        (*print_pages_params_).params, frame, node, frame->view());
    expected_pages_count = prep_frame_view.GetExpectedPageCount();
    if (expected_pages_count)
      use_browser_overlays = prep_frame_view.ShouldUseBrowserOverlays();
  }

  // Some full screen plugins can say they don't want to print.
  if (expected_pages_count) {
    if (!is_preview_) {
      // Ask the browser to show UI to retrieve the final print settings.
      if (!GetPrintSettingsFromUser(frame, expected_pages_count,
                                    use_browser_overlays)) {
        print_cancelled = true;
      }
    }

    // Render Pages for printing.
    if (!print_cancelled) {
      if (is_preview_)
        RenderPagesForPreview(frame);
      else
        RenderPagesForPrint(frame, node);

      // Reset cancel counter on first successful print.
      user_cancelled_scripted_print_count_ = 0;
      return;  // All went well.
    } else {
      if (script_initiated) {
        ++user_cancelled_scripted_print_count_;
        last_cancelled_script_print_ = base::Time::Now();
      }
    }
  } else {
    // Nothing to print.
    print_cancelled = true;
  }
  // When |print_cancelled| is true, we treat it as success so that
  // DidFinishPrinting() won't show any error alert. we call
  // DidFinishPrinting() here to release printing resources, since
  // we don't need them anymore.
  if (print_cancelled)
    DidFinishPrinting(print_cancelled);
}

void PrintWebViewHelper::DidFinishPrinting(bool success) {
  if (!success) {
    WebView* web_view = print_web_view_;
    if (!web_view)
      web_view = render_view()->webview();

    render_view()->runModalAlertDialog(
        web_view->mainFrame(),
        l10n_util::GetStringUTF16(IDS_PRINT_SPOOL_FAILED_ERROR_TEXT));
  }

  if (print_web_view_) {
    print_web_view_->close();
    print_web_view_ = NULL;
  }
  print_pages_params_.reset();
}

bool PrintWebViewHelper::CopyAndPrint(WebFrame* web_frame) {
  // Create a new WebView with the same settings as the current display one.
  // Except that we disable javascript (don't want any active content running
  // on the page).
  WebPreferences prefs = render_view()->webkit_preferences();
  prefs.javascript_enabled = false;
  prefs.java_enabled = false;

  print_web_view_ = WebView::create(this, NULL, NULL);
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
void PrintWebViewHelper::PrintPages(const ViewMsg_PrintPages_Params& params,
                                    WebFrame* frame,
                                    WebNode* node) {
  ViewMsg_Print_Params printParams = params.params;
  UpdatePrintableSizeInPrintParameters(frame, node, &printParams);

  PrepareFrameAndViewForPrint prep_frame_view(printParams,
                                              frame,
                                              node,
                                              frame->view());
  int page_count = prep_frame_view.GetExpectedPageCount();

  render_view()->Send(new ViewHostMsg_DidGetPrintedPagesCount(
      render_view()->routing_id(), printParams.document_cookie, page_count));
  if (!page_count)
    return;

  const gfx::Size& canvas_size = prep_frame_view.GetPrintCanvasSize();
  ViewMsg_PrintPage_Params page_params;
  page_params.params = printParams;
  if (params.pages.empty()) {
    for (int i = 0; i < page_count; ++i) {
      page_params.page_number = i;
      PrintPage(page_params, canvas_size, frame);
    }
  } else {
    for (size_t i = 0; i < params.pages.size(); ++i) {
      if (params.pages[i] >= page_count)
        break;
      page_params.page_number = params.pages[i];
      PrintPage(page_params, canvas_size, frame);
    }
  }
}
#endif  // OS_MACOSX || OS_WIN

void PrintWebViewHelper::didStopLoading() {
  DCHECK(print_pages_params_.get() != NULL);
  PrintPages(*print_pages_params_.get(), print_web_view_->mainFrame(), NULL);
}

void PrintWebViewHelper::GetPageSizeAndMarginsInPoints(
    WebFrame* frame,
    int page_index,
    const ViewMsg_Print_Params& default_params,
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

void PrintWebViewHelper::UpdatePrintableSizeInPrintParameters(
    WebFrame* frame,
    WebNode* node,
    ViewMsg_Print_Params* params) {
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

bool PrintWebViewHelper::InitPrintSettings(WebFrame* frame,
                                           WebNode* node) {
  ViewMsg_PrintPages_Params settings;
  if (GetDefaultPrintSettings(frame, node, &settings.params)) {
    print_pages_params_.reset(new ViewMsg_PrintPages_Params(settings));
    print_pages_params_->pages.clear();
    return true;
  }
  return false;
}

bool PrintWebViewHelper::GetDefaultPrintSettings(
    WebFrame* frame,
    WebNode* node,
    ViewMsg_Print_Params* params) {
  if (!render_view()->Send(new ViewHostMsg_GetDefaultPrintSettings(
          render_view()->routing_id(), params))) {
    NOTREACHED();
    return false;
  }
  // Check if the printer returned any settings, if the settings is empty, we
  // can safely assume there are no printer drivers configured. So we safely
  // terminate.
  if (params->IsEmpty()) {
    render_view()->runModalAlertDialog(
        frame,
        l10n_util::GetStringUTF16(IDS_DEFAULT_PRINTER_NOT_FOUND_WARNING));
    return false;
  }
  if (!(params->dpi && params->document_cookie)) {
    // Invalid print page settings.
    NOTREACHED();
    return false;
  }
  UpdatePrintableSizeInPrintParameters(frame, node, params);
  return true;
}

bool PrintWebViewHelper::GetPrintSettingsFromUser(WebFrame* frame,
                                                  int expected_pages_count,
                                                  bool use_browser_overlays) {
  ViewHostMsg_ScriptedPrint_Params params;
  ViewMsg_PrintPages_Params print_settings;

  // The routing id is sent across as it is needed to look up the
  // corresponding RenderViewHost instance to signal and reset the
  // pump messages event.
  params.routing_id = render_view()->routing_id();
  // host_window_ may be NULL at this point if the current window is a
  // popup and the print() command has been issued from the parent. The
  // receiver of this message has to deal with this.
  params.host_window_id = render_view()->host_window();
  params.cookie = (*print_pages_params_).params.document_cookie;
  params.has_selection = frame->hasSelection();
  params.expected_pages_count = expected_pages_count;
  params.use_overlays = use_browser_overlays;

  print_pages_params_.reset();
  IPC::SyncMessage* msg = new ViewHostMsg_ScriptedPrint(
      render_view()->routing_id(), params, &print_settings);
  msg->EnableMessagePumping();
  if (render_view()->Send(msg)) {
    print_pages_params_.reset(new ViewMsg_PrintPages_Params(print_settings));
  } else {
    // Send() failed.
    NOTREACHED();
    return false;
  }
  return (print_settings.params.dpi && print_settings.params.document_cookie);
}

void PrintWebViewHelper::RenderPagesForPrint(WebFrame* frame,
                                             WebNode* node) {
  ViewMsg_PrintPages_Params print_settings = *print_pages_params_;
  if (print_settings.params.selection_only) {
    CopyAndPrint(frame);
  } else {
    // TODO: Always copy before printing.
    PrintPages(print_settings, frame, node);
  }
}

void PrintWebViewHelper::RenderPagesForPreview(WebFrame *frame) {
  ViewMsg_PrintPages_Params print_settings = *print_pages_params_;
  // TODO(kmadhusu): Handle print selection.
  CreatePreviewDocument(print_settings, frame);
}

#if defined(OS_POSIX)
bool PrintWebViewHelper::CopyMetafileDataToSharedMem(
    printing::NativeMetafile* metafile,
    base::SharedMemoryHandle* shared_mem_handle) {
  uint32 buf_size = metafile->GetDataSize();
  base::SharedMemoryHandle mem_handle;
  if (render_view()->Send(
          new ViewHostMsg_AllocateSharedMemoryBuffer(buf_size, &mem_handle))) {
    if (base::SharedMemory::IsHandleValid(mem_handle)) {
      base::SharedMemory shared_buf(mem_handle, false);
      if (shared_buf.Map(buf_size)) {
        metafile->GetData(shared_buf.memory(), buf_size);
        shared_buf.GiveToProcess(base::GetCurrentProcessHandle(),
                                 shared_mem_handle);
        return true;
      }
    }
  }
  NOTREACHED();
  return false;
}
#endif  // defined(OS_POSIX)
