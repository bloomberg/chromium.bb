// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/print_web_view_helper.h"

#include "app/gfx/codec/jpeg_codec.h"
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
#include "webkit/glue/webkit_glue.h"

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
      print_web_view_(NULL),
      user_cancelled_scripted_print_count_(0) {}

PrintWebViewHelper::~PrintWebViewHelper() {}

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
