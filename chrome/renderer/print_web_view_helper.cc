// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/print_web_view_helper.h"

#include "app/l10n_util.h"
#include "base/logging.h"
#include "chrome/common/render_messages.h"
#include "chrome/renderer/render_view.h"
#include "gfx/codec/jpeg_codec.h"
#include "grit/generated_resources.h"
#include "printing/units.h"
#include "third_party/WebKit/WebKit/chromium/public/WebConsoleMessage.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/WebKit/chromium/public/WebRect.h"
#include "third_party/WebKit/WebKit/chromium/public/WebScreenInfo.h"
#include "third_party/WebKit/WebKit/chromium/public/WebSize.h"
#include "third_party/WebKit/WebKit/chromium/public/WebURLRequest.h"
#include "webkit/glue/webkit_glue.h"

using WebKit::WebConsoleMessage;
using WebKit::WebFrame;
using WebKit::WebRect;
using WebKit::WebScreenInfo;
using WebKit::WebString;
using WebKit::WebURLRequest;
using WebKit::WebView;

PrepareFrameAndViewForPrint::PrepareFrameAndViewForPrint(
    const ViewMsg_Print_Params& print_params,
    WebFrame* frame,
    WebView* web_view)
        : frame_(frame), web_view_(web_view), expected_pages_count_(0),
          use_browser_overlays_(true) {
  print_canvas_size_.set_width(
      printing::ConvertUnit(print_params.printable_size.width(),
                            static_cast<int>(print_params.dpi),
                            print_params.desired_dpi));

  print_canvas_size_.set_height(
      printing::ConvertUnit(print_params.printable_size.height(),
                            static_cast<int>(print_params.dpi),
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

  expected_pages_count_ = frame->printBegin(
      print_canvas_size_, static_cast<int>(print_params.dpi),
      &use_browser_overlays_);
}

PrepareFrameAndViewForPrint::~PrepareFrameAndViewForPrint() {
  frame_->printEnd();
  web_view_->resize(prev_view_size_);
}


PrintWebViewHelper::PrintWebViewHelper(RenderView* render_view)
    : render_view_(render_view),
      print_web_view_(NULL),
      user_cancelled_scripted_print_count_(0) {}

PrintWebViewHelper::~PrintWebViewHelper() {}

#if defined(OS_MACOSX) || defined(OS_WIN)
void PrintWebViewHelper::Print(WebFrame* frame, bool script_initiated) {
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

  // Retrieve the default print settings to calculate the expected number of
  // pages.
  ViewMsg_Print_Params default_settings;
  bool user_cancelled_print = false;

  IPC::SyncMessage* msg =
      new ViewHostMsg_GetDefaultPrintSettings(routing_id(), &default_settings);
  if (Send(msg)) {
    msg = NULL;
    // Check if the printer returned any settings, if the settings is empty, we
    // can safely assume there are no printer drivers configured. So we safely
    // terminate.
    if (default_settings.IsEmpty()) {
      // TODO: Create an async alert (http://crbug.com/14918).
      render_view_->runModalAlertDialog(frame,
          l10n_util::GetStringUTF16(IDS_DEFAULT_PRINTER_NOT_FOUND_WARNING));
      return;
    }

    // Continue only if the settings are valid.
    if (default_settings.dpi && default_settings.document_cookie) {
      int expected_pages_count = 0;
      bool use_browser_overlays = true;

      // Prepare once to calculate the estimated page count.  This must be in
      // a scope for itself (see comments on PrepareFrameAndViewForPrint).
      {
        PrepareFrameAndViewForPrint prep_frame_view(default_settings,
                                                    frame,
                                                    frame->view());
        expected_pages_count = prep_frame_view.GetExpectedPageCount();
        DCHECK(expected_pages_count);
        use_browser_overlays = prep_frame_view.ShouldUseBrowserOverlays();
      }

      // Ask the browser to show UI to retrieve the final print settings.
      ViewMsg_PrintPages_Params print_settings;

      ViewHostMsg_ScriptedPrint_Params params;

      // The routing id is sent across as it is needed to look up the
      // corresponding RenderViewHost instance to signal and reset the
      // pump messages event.
      params.routing_id = routing_id();
      // host_window_ may be NULL at this point if the current window is a popup
      // and the print() command has been issued from the parent. The receiver
      // of this message has to deal with this.
      params.host_window_id = render_view_->host_window();
      params.cookie = default_settings.document_cookie;
      // TODO(maruel): Reenable once http://crbug.com/22937 is fixed.
      // Print selection is broken because DidStopLoading is never called.
      // params.has_selection = frame->hasSelection();
      params.has_selection = false;
      params.expected_pages_count = expected_pages_count;
      params.use_overlays = use_browser_overlays;

      msg = new ViewHostMsg_ScriptedPrint(routing_id(), params,
                                          &print_settings);
      msg->EnableMessagePumping();
      if (Send(msg)) {
        msg = NULL;

        // If the settings are invalid, early quit.
        if (print_settings.params.dpi &&
            print_settings.params.document_cookie) {
          if (print_settings.params.selection_only) {
            CopyAndPrint(print_settings, frame);
          } else {
            // TODO: Always copy before printing.
            PrintPages(print_settings, frame);
          }

          // Reset cancel counter on first successful print.
          user_cancelled_scripted_print_count_ = 0;
          return;  // All went well.
        } else {
          user_cancelled_print = true;
        }
      } else {
        // Send() failed.
        NOTREACHED();
      }
    } else {
      // Failed to get default settings.
      NOTREACHED();
    }
  } else {
    // Send() failed.
    NOTREACHED();
  }
  if (script_initiated && user_cancelled_print) {
    ++user_cancelled_scripted_print_count_;
    last_cancelled_script_print_ = base::Time::Now();
  }
  // When |user_cancelled_print| is true, we treat it as success so that
  // DidFinishPrinting() won't show any error alert.
  // If |user_cancelled_print| is false and we reach here, there must be
  // something wrong and hence is not success, DidFinishPrinting() should show
  // an error alert.
  // In both cases, we have to call DidFinishPrinting() here to release
  // printing resources, since we don't need them anymore.
  DidFinishPrinting(user_cancelled_print);
}
#endif  // OS_MACOSX || OS_WIN

void PrintWebViewHelper::DidFinishPrinting(bool success) {
  if (!success) {
    WebView* web_view = print_web_view_;
    if (!web_view)
      web_view = render_view_->webview();

    // TODO: Create an async alert (http://crbug.com/14918).
    render_view_->runModalAlertDialog(
        web_view->mainFrame(),
        WideToUTF16Hack(
            l10n_util::GetString(IDS_PRINT_SPOOL_FAILED_ERROR_TEXT)));
  }

  if (print_web_view_) {
    print_web_view_->close();
    print_web_view_ = NULL;
    print_pages_params_.reset();
  }
}

bool PrintWebViewHelper::CopyAndPrint(const ViewMsg_PrintPages_Params& params,
                                      WebFrame* web_frame) {
  // Create a new WebView with the same settings as the current display one.
  // Except that we disable javascript (don't want any active content running
  // on the page).
  WebPreferences prefs = render_view_->webkit_preferences();
  prefs.javascript_enabled = false;
  prefs.java_enabled = false;

  print_web_view_ = WebView::create(this);
  prefs.Apply(print_web_view_);
  print_web_view_->initializeMainFrame(NULL);

  print_pages_params_.reset(new ViewMsg_PrintPages_Params(params));
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

void PrintWebViewHelper::PrintPageAsJPEG(
    const ViewMsg_PrintPage_Params& params,
    WebFrame* frame,
    float zoom_factor,
    std::vector<unsigned char>* image_data) {
  PrepareFrameAndViewForPrint prep_frame_view(params.params,
                                              frame,
                                              frame->view());
  const gfx::Size& canvas_size(prep_frame_view.GetPrintCanvasSize());

  // Since WebKit extends the page width depending on the magical shrink
  // factor we make sure the canvas covers the worst case scenario
  // (x2.0 currently).  PrintContext will then set the correct clipping region.
  int size_x = static_cast<int>(canvas_size.width() * params.params.max_shrink);
  int size_y = static_cast<int>(canvas_size.height() *
      params.params.max_shrink);

  // Access the bitmap from the canvas device.
  skia::PlatformCanvas canvas(size_x, size_y, true);
  frame->printPage(params.page_number, webkit_glue::ToWebCanvas(&canvas));
  const SkBitmap& bitmap = canvas.getDevice()->accessBitmap(false);

  // Encode the SkBitmap to jpeg.
  SkAutoLockPixels image_lock(bitmap);
  bool encoded = gfx::JPEGCodec::Encode(
      reinterpret_cast<unsigned char*>(bitmap.getAddr32(0, 0)),
      gfx::JPEGCodec::FORMAT_BGRA,
      static_cast<int>(bitmap.width() * zoom_factor),
      static_cast<int>(bitmap.height() * zoom_factor),
      static_cast<int>(bitmap.rowBytes()),
      90,
      image_data);
  DCHECK(encoded);
}

#if defined(OS_MACOSX) || defined(OS_WIN)
void PrintWebViewHelper::PrintPages(const ViewMsg_PrintPages_Params& params,
                                    WebFrame* frame) {
  PrepareFrameAndViewForPrint prep_frame_view(params.params,
                                              frame,
                                              frame->view());
  int page_count = prep_frame_view.GetExpectedPageCount();

  Send(new ViewHostMsg_DidGetPrintedPagesCount(routing_id(),
                                               params.params.document_cookie,
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
  PrintPages(*print_pages_params_.get(), print_web_view_->mainFrame());
}
