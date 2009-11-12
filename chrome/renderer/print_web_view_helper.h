// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_PRINT_WEB_VIEW_HELPER_H_
#define CHROME_RENDERER_PRINT_WEB_VIEW_HELPER_H_

#include <vector>

#include "base/gfx/size.h"
#include "base/scoped_ptr.h"
#include "base/time.h"
#include "third_party/WebKit/WebKit/chromium/public/WebViewClient.h"

namespace gfx {
class Size;
}

namespace IPC {
class Message;
}

#if defined(OS_LINUX)
namespace printing {
class PdfPsMetafile;
typedef PdfPsMetafile NativeMetafile;
}
#endif

class RenderView;
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
                              WebKit::WebFrame* frame,
                              WebKit::WebView* web_view);
  ~PrepareFrameAndViewForPrint();

  int GetExpectedPageCount() const {
    return expected_pages_count_;
  }

  const gfx::Size& GetPrintCanvasSize() const {
    return print_canvas_size_;
  }

 private:
  WebKit::WebFrame* frame_;
  WebKit::WebView* web_view_;
  gfx::Size print_canvas_size_;
  gfx::Size prev_view_size_;
  int expected_pages_count_;

  DISALLOW_COPY_AND_ASSIGN(PrepareFrameAndViewForPrint);
};


// PrintWebViewHelper handles most of the printing grunt work for RenderView.
// We plan on making print asynchronous and that will require copying the DOM
// of the document and creating a new WebView with the contents.
class PrintWebViewHelper : public WebKit::WebViewClient {
 public:
  explicit PrintWebViewHelper(RenderView* render_view);
  virtual ~PrintWebViewHelper();

  void Print(WebKit::WebFrame* frame, bool script_initiated);

  // Is there a background print in progress?
  bool IsPrinting() {
    return print_web_view_ != NULL;
  }

  // Notification when printing is done - signal teardown
  void DidFinishPrinting(bool success);

  // Prints the page listed in |params| as a JPEG image. The width and height of
  // the image will scale propotionally given the |zoom_factor| multiplier. The
  // encoded JPEG data will be written into the supplied vector |image_data|.
  void PrintPageAsJPEG(const ViewMsg_PrintPage_Params& params,
                       WebKit::WebFrame* frame,
                       float zoom_factor,
                       std::vector<unsigned char>* image_data);

 protected:
  bool CopyAndPrint(const ViewMsg_PrintPages_Params& params,
                    WebKit::WebFrame* web_frame);

  // Prints the page listed in |params|.
#if defined(OS_LINUX)
  void PrintPage(const ViewMsg_PrintPage_Params& params,
                 const gfx::Size& canvas_size,
                 WebKit::WebFrame* frame,
                 printing::NativeMetafile* metafile);
#else
  void PrintPage(const ViewMsg_PrintPage_Params& params,
                 const gfx::Size& canvas_size,
                 WebKit::WebFrame* frame);
#endif

  // Prints all the pages listed in |params|.
  // It will implicitly revert the document to display CSS media type.
  void PrintPages(const ViewMsg_PrintPages_Params& params,
                  WebKit::WebFrame* frame);

  // IPC::Message::Sender
  bool Send(IPC::Message* msg);

  int32 routing_id();

  // WebKit::WebViewClient override:
  virtual void didStopLoading();

 private:
  RenderView* render_view_;
  WebKit::WebView* print_web_view_;
  scoped_ptr<ViewMsg_PrintPages_Params> print_pages_params_;
  base::Time last_cancelled_script_print_;
  int user_cancelled_scripted_print_count_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PrintWebViewHelper);
};

#endif  // CHROME_RENDERER_PRINT_WEB_VIEW_HELPER_H_
