// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/print_web_view_helper.h"

#include "app/l10n_util.h"
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
#include "webkit/glue/webkit_glue.h"

using printing::ConvertPixelsToPoint;
using printing::ConvertPixelsToPointDouble;
using printing::ConvertUnit;
using printing::ConvertUnitDouble;
using WebKit::WebConsoleMessage;
using WebKit::WebFrame;
using WebKit::WebNode;
using WebKit::WebRect;
using WebKit::WebSize;
using WebKit::WebScreenInfo;
using WebKit::WebString;
using WebKit::WebURLRequest;
using WebKit::WebView;

PrepareFrameAndViewForPrint::PrepareFrameAndViewForPrint(
    const ViewMsg_Print_Params& print_params,
    WebFrame* frame,
    WebNode* node,
    WebView* web_view)
        : frame_(frame), web_view_(web_view), expected_pages_count_(0),
          use_browser_overlays_(true) {
  int dpi = static_cast<int>(print_params.dpi);
#if defined(OS_MACOSX)
  // On the Mac, the printable area is in points, don't do any scaling based
  // on dpi.
  dpi = printing::kPointsPerInch;
#endif  // defined(OS_MACOSX)
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
}


PrintWebViewHelper::PrintWebViewHelper(RenderView* render_view)
    : render_view_(render_view),
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

void PrintWebViewHelper::Print(WebKit::WebFrame* frame,
                               WebNode* node,
                               bool script_initiated,
                               bool is_preview) {
  const int kMinSecondsToIgnoreJavascriptInitiatedPrint = 2;
  const int kMaxSecondsToIgnoreJavascriptInitiatedPrint = 2 * 60;  // 2 Minutes.

  // If still not finished with earlier print request simply ignore.
  if (IsPrinting())
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
      web_view = render_view_->webview();

    render_view_->runModalAlertDialog(
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
  WebPreferences prefs = render_view_->webkit_preferences();
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

  Send(new ViewHostMsg_DidGetPrintedPagesCount(routing_id(),
                                               printParams.document_cookie,
                                               page_count));
  if (!page_count)
    return;

  const gfx::Size& canvas_size = prep_frame_view.GetPrintCanvasSize();
  ViewMsg_PrintPage_Params page_params;
  page_params.params = params.params;
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

bool PrintWebViewHelper::Send(IPC::Message* msg) {
  return render_view_->Send(msg);
}

int32 PrintWebViewHelper::routing_id() {
  return render_view_->routing_id();
}

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
  int dpi = static_cast<int>(default_params.dpi);
#if defined(OS_MACOSX)
  // On the Mac, the printable area is in points, don't do any scaling based
  // on dpi.
  dpi = printing::kPointsPerInch;
#endif

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
#if defined(OS_MACOSX)
  // On the Mac, the printable area is in points, don't do any scaling based
  // on dpi.
  int dpi = printing::kPointsPerInch;
#else
  int dpi = static_cast<int>(params->dpi);
#endif  // defined(OS_MACOSX)
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
  IPC::SyncMessage* msg =
      new ViewHostMsg_GetDefaultPrintSettings(routing_id(), params);
  if (!Send(msg)) {
    NOTREACHED();
    return false;
  }
  // Check if the printer returned any settings, if the settings is empty, we
  // can safely assume there are no printer drivers configured. So we safely
  // terminate.
  if (params->IsEmpty()) {
    render_view_->runModalAlertDialog(
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
  params.routing_id = routing_id();
  // host_window_ may be NULL at this point if the current window is a
  // popup and the print() command has been issued from the parent. The
  // receiver of this message has to deal with this.
  params.host_window_id = render_view_->host_window();
  params.cookie = (*print_pages_params_).params.document_cookie;
  params.has_selection = frame->hasSelection();
  params.expected_pages_count = expected_pages_count;
  params.use_overlays = use_browser_overlays;

  print_pages_params_.reset();
  IPC::SyncMessage* msg =
      new ViewHostMsg_ScriptedPrint(routing_id(), params, &print_settings);
  msg->EnableMessagePumping();
  if (Send(msg)) {
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
  ViewHostMsg_DidPreviewDocument_Params print_params;
  CreatePreviewDocument(print_settings, frame, &print_params);
  Send(new ViewHostMsg_PagesReadyForPreview(routing_id(), print_params));
}

#if !defined(OS_MACOSX)
void PrintWebViewHelper::CreatePreviewDocument(
    const ViewMsg_PrintPages_Params& params,
    WebFrame* frame,
    ViewHostMsg_DidPreviewDocument_Params* print_params) {
  // TODO(kmadhusu): Implement this function for windows & linux.
  print_params->document_cookie = params.params.document_cookie;
}
#endif

#if defined(OS_MACOSX)
bool PrintWebViewHelper::CopyMetafileDataToSharedMem(
    printing::NativeMetafile* metafile,
    base::SharedMemoryHandle* shared_mem_handle) {
  uint32 buf_size = metafile->GetDataSize();
  base::SharedMemoryHandle mem_handle;
  if (Send(new ViewHostMsg_AllocateSharedMemoryBuffer(buf_size, &mem_handle))) {
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
#endif
