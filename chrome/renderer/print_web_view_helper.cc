// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/print_web_view_helper.h"

#include "app/l10n_util.h"
#include "base/logging.h"
#include "chrome/common/render_messages.h"
#include "chrome/renderer/render_view.h"
#include "grit/generated_resources.h"
#include "printing/units.h"
#include "webkit/api/public/WebFrame.h"
#include "webkit/api/public/WebRect.h"
#include "webkit/api/public/WebScreenInfo.h"
#include "webkit/api/public/WebSize.h"
#include "webkit/api/public/WebURLRequest.h"

using WebKit::WebFrame;
using WebKit::WebRect;
using WebKit::WebScreenInfo;
using WebKit::WebString;
using WebKit::WebURLRequest;

PrepareFrameAndViewForPrint::PrepareFrameAndViewForPrint(
    const ViewMsg_Print_Params& print_params,
    WebFrame* frame,
    WebView* web_view)
        : frame_(frame), web_view_(web_view), expected_pages_count_(0) {

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

  expected_pages_count_ = frame->printBegin(print_canvas_size_);
}

PrepareFrameAndViewForPrint::~PrepareFrameAndViewForPrint() {
  frame_->printEnd();
  web_view_->resize(prev_view_size_);
}


PrintWebViewHelper::PrintWebViewHelper(RenderView* render_view)
    : render_view_(render_view),
      user_cancelled_scripted_print_count_(0) {}

PrintWebViewHelper::~PrintWebViewHelper() {}

void PrintWebViewHelper::DidFinishPrinting(bool success) {
  if (!success) {
    WebView* web_view = print_web_view_.get();
    if (!web_view)
      web_view = render_view_->webview();

    // TODO: Create an async alert (http://crbug.com/14918).
    render_view_->RunJavaScriptAlert(web_view->GetMainFrame(),
        l10n_util::GetString(IDS_PRINT_SPOOL_FAILED_ERROR_TEXT));
  }

  if (print_web_view_.get()) {
    print_web_view_->close();
    print_web_view_.release();  // Close deletes object.
    print_pages_params_.reset();
  }
}

bool PrintWebViewHelper::CopyAndPrint(const ViewMsg_PrintPages_Params& params,
                                      WebFrame* web_frame) {
  // Create a new WebView with the same settings as the current display one.
  // Except that we disable javascript (don't want any active content running
  // on the page).
  WebPreferences prefs = web_frame->view()->GetPreferences();
  prefs.javascript_enabled = false;
  prefs.java_enabled = false;
  print_web_view_.reset(WebView::Create(this, prefs));

  print_pages_params_.reset(new ViewMsg_PrintPages_Params(params));
  print_pages_params_->pages.clear();  // Print all pages of selection.

  std::string html = web_frame->selectionAsMarkup().utf8();
  std::string url_str = "data:text/html;charset=utf-8,";
  url_str.append(html);
  GURL url(url_str);

  // When loading is done this will call DidStopLoading that will do the
  // actual printing.
  print_web_view_->GetMainFrame()->loadRequest(WebURLRequest(url));

  return true;
}

void PrintWebViewHelper::PrintPages(const ViewMsg_PrintPages_Params& params,
                                    WebFrame* frame) {
  PrepareFrameAndViewForPrint prep_frame_view(params.params,
                                              frame,
                                              frame->view());
  int page_count = prep_frame_view.GetExpectedPageCount();

  Send(new ViewHostMsg_DidGetPrintedPagesCount(routing_id(),
                                               params.params.document_cookie,
                                               page_count));
  if (page_count) {
    ViewMsg_PrintPage_Params page_params;
    page_params.params = params.params;
    if (params.pages.empty()) {
      for (int i = 0; i < page_count; ++i) {
        page_params.page_number = i;
        PrintPage(page_params, prep_frame_view.GetPrintCanvasSize(), frame);
      }
    } else {
      for (size_t i = 0; i < params.pages.size(); ++i) {
        page_params.page_number = params.pages[i];
        PrintPage(page_params, prep_frame_view.GetPrintCanvasSize(), frame);
      }
    }
  }
}

bool PrintWebViewHelper::Send(IPC::Message* msg) {
  return render_view_->Send(msg);
}

int32 PrintWebViewHelper::routing_id() {
  return render_view_->routing_id();
}

WebRect PrintWebViewHelper::windowRect() {
  NOTREACHED();
  return WebRect();
}

WebRect PrintWebViewHelper::windowResizerRect() {
  NOTREACHED();
  return WebRect();
}

WebRect PrintWebViewHelper::rootWindowRect() {
  NOTREACHED();
  return WebRect();
}

WebScreenInfo PrintWebViewHelper::screenInfo() {
  NOTREACHED();
  return WebScreenInfo();
}

void PrintWebViewHelper::DidStopLoading(WebView* webview) {
  DCHECK(print_pages_params_.get() != NULL);
  DCHECK_EQ(webview, print_web_view_.get());
  PrintPages(*print_pages_params_.get(), print_web_view_->GetMainFrame());
}
