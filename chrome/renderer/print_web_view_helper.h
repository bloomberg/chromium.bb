// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_PRINT_WEB_VIEW_DELEGATE_H_
#define CHROME_RENDERER_PRINT_WEB_VIEW_DELEGATE_H_

#include <vector>

#include "base/gfx/size.h"
#include "base/scoped_ptr.h"
#include "base/time.h"
#include "webkit/glue/webview_delegate.h"

namespace gfx {
class Size;
}

namespace IPC {
class Message;
}

class RenderView;
class WebView;
struct ViewMsg_Print_Params;
struct ViewMsg_PrintPage_Params;
struct ViewMsg_PrintPages_Params;


// Class that calls the Begin and End print functions on the frame and changes
// the size of the view temporarily to support full page printing..
// Do not serve any events in the time between construction and destruction of
// this class because it will cause flicker.
class PrepareFrameAndViewForPrint {
 public:
  PrepareFrameAndViewForPrint(const ViewMsg_Print_Params& print_params,
                              WebFrame* frame,
                              WebView* web_view);
  ~PrepareFrameAndViewForPrint();

  int GetExpectedPageCount() const {
    return expected_pages_count_;
  }

  const gfx::Size& GetPrintCanvasSize() const {
    return print_canvas_size_;
  }

 private:
  WebFrame* frame_;
  WebView* web_view_;
  gfx::Size print_canvas_size_;
  gfx::Size prev_view_size_;
  int expected_pages_count_;

  DISALLOW_COPY_AND_ASSIGN(PrepareFrameAndViewForPrint);
};


// PrintWebViewHelper handles most of the printing grunt work for RenderView.
// We plan on making print asynchronous and that will require copying the DOM
// of the document and creating a new WebView with the contents.
class PrintWebViewHelper : public WebViewDelegate {
 public:
  explicit PrintWebViewHelper(RenderView* render_view);
  virtual ~PrintWebViewHelper();

  void Print(WebFrame* frame, bool script_initiated);

  // Is there a background print in progress?
  bool IsPrinting() {
    return print_web_view_.get() != NULL;
  }

  // Notification when printing is done - signal teardown
  void DidFinishPrinting(bool success);

 protected:
  bool CopyAndPrint(const ViewMsg_PrintPages_Params& params,
                    WebFrame* web_frame);

  // Prints the page listed in |params|.
  void PrintPage(const ViewMsg_PrintPage_Params& params,
                 const gfx::Size& canvas_size,
                 WebFrame* frame);

  // Prints all the pages listed in |params|.
  // It will implicitly revert the document to display CSS media type.
  void PrintPages(const ViewMsg_PrintPages_Params& params, WebFrame* frame);

  // IPC::Message::Sender
  bool Send(IPC::Message* msg);

  int32 routing_id();

  // WebViewDeletegate
  virtual void didInvalidateRect(const WebKit::WebRect&) {}
  virtual void didScrollRect(int dx, int dy, const WebKit::WebRect& clipRect) {}
  virtual void didFocus() {}
  virtual void didBlur() {}
  virtual void didChangeCursor(const WebKit::WebCursorInfo&) {}
  virtual void closeWidgetSoon() {}
  virtual void show(WebKit::WebNavigationPolicy) {}
  virtual void runModal() {}
  virtual WebKit::WebRect windowRect();
  virtual void setWindowRect(const WebKit::WebRect&) {}
  virtual WebKit::WebRect windowResizerRect();
  virtual WebKit::WebRect rootWindowRect();
  virtual WebKit::WebScreenInfo screenInfo();
  virtual void DidStopLoading(WebView* webview);

 private:
  RenderView* render_view_;
  scoped_ptr<WebView> print_web_view_;
  scoped_ptr<ViewMsg_PrintPages_Params> print_pages_params_;
  base::Time last_cancelled_script_print_;
  int user_cancelled_scripted_print_count_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PrintWebViewHelper);
};

#endif  // CHROME_RENDERER_PRINT_WEB_VIEW_DELEGATE_H_
