// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_PRINT_WEB_VIEW_HELPER_H_
#define CHROME_RENDERER_PRINT_WEB_VIEW_HELPER_H_
#pragma once

#include <vector>

#include "base/scoped_ptr.h"
#include "base/time.h"
#include "printing/native_metafile.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrameClient.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebViewClient.h"
#include "ui/gfx/size.h"

#if defined(OS_MACOSX) || defined(OS_WIN)
#include "base/shared_memory.h"
#endif  // defined(OS_MACOSX) || defined(OS_WIN)

namespace gfx {
class Size;
}

namespace IPC {
class Message;
}

#if defined(USE_X11)
namespace skia {
class VectorCanvas;
}
#endif

class RenderView;
struct ViewMsg_Print_Params;
struct ViewMsg_PrintPage_Params;
struct ViewMsg_PrintPages_Params;
struct ViewHostMsg_DidPreviewDocument_Params;

// Class that calls the Begin and End print functions on the frame and changes
// the size of the view temporarily to support full page printing..
// Do not serve any events in the time between construction and destruction of
// this class because it will cause flicker.
class PrepareFrameAndViewForPrint {
 public:
  // Prints |frame|.  If |node| is not NULL, then only that node will be
  // printed.
  PrepareFrameAndViewForPrint(const ViewMsg_Print_Params& print_params,
                              WebKit::WebFrame* frame,
                              WebKit::WebNode* node,
                              WebKit::WebView* web_view);
  ~PrepareFrameAndViewForPrint();

  int GetExpectedPageCount() const {
    return expected_pages_count_;
  }

  bool ShouldUseBrowserOverlays() const {
    return use_browser_overlays_;
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
  bool use_browser_overlays_;

  DISALLOW_COPY_AND_ASSIGN(PrepareFrameAndViewForPrint);
};


// PrintWebViewHelper handles most of the printing grunt work for RenderView.
// We plan on making print asynchronous and that will require copying the DOM
// of the document and creating a new WebView with the contents.
class PrintWebViewHelper : public WebKit::WebViewClient,
                           public WebKit::WebFrameClient {
 public:
  explicit PrintWebViewHelper(RenderView* render_view);
  virtual ~PrintWebViewHelper();

  void PrintFrame(WebKit::WebFrame* frame,
                  bool script_initiated,
                  bool is_preview);

  void PrintNode(WebKit::WebNode* node,
                 bool script_initiated,
                 bool is_preview);

  // Is there a background print in progress?
  bool IsPrinting() {
    return print_web_view_ != NULL;
  }

  // Notification when printing is done - signal teardown
  void DidFinishPrinting(bool success);

 protected:
  bool CopyAndPrint(WebKit::WebFrame* web_frame);

  // Prints the page listed in |params|.
#if defined(USE_X11)
  void PrintPage(const ViewMsg_PrintPage_Params& params,
                 const gfx::Size& canvas_size,
                 WebKit::WebFrame* frame,
                 printing::NativeMetafile* metafile,
                 skia::VectorCanvas** canvas);
#else
  void PrintPage(const ViewMsg_PrintPage_Params& params,
                 const gfx::Size& canvas_size,
                 WebKit::WebFrame* frame);
#endif

  // Prints all the pages listed in |params|.
  // It will implicitly revert the document to display CSS media type.
  void PrintPages(const ViewMsg_PrintPages_Params& params,
                  WebKit::WebFrame* frame,
                  WebKit::WebNode* node);

  // IPC::Message::Sender
  bool Send(IPC::Message* msg);

  int32 routing_id();

  // WebKit::WebViewClient override:
  virtual void didStopLoading();

 private:
  static void GetPageSizeAndMarginsInPoints(
      WebKit::WebFrame* frame,
      int page_index,
      const ViewMsg_Print_Params& default_params,
      double* content_width_in_points,
      double* content_height_in_points,
      double* margin_top_in_points,
      double* margin_right_in_points,
      double* margin_bottom_in_points,
      double* margin_left_in_points);

  void Print(WebKit::WebFrame* frame,
             WebKit::WebNode* node,
             bool script_initiated,
             bool is_preview);

  void UpdatePrintableSizeInPrintParameters(WebKit::WebFrame* frame,
                                            WebKit::WebNode* node,
                                            ViewMsg_Print_Params* params);

  // Initialize print page settings with default settings.
  bool InitPrintSettings(WebKit::WebFrame* frame,
                         WebKit::WebNode* node);

  // Get the default printer settings.
  bool GetDefaultPrintSettings(WebKit::WebFrame* frame,
                               WebKit::WebNode* node,
                               ViewMsg_Print_Params* params);

  // Get final print settings from the user.
  // Return false if the user cancels or on error.
  bool GetPrintSettingsFromUser(WebKit::WebFrame* frame,
                                int expected_pages_count,
                                bool use_browser_overlays);

  // Render the frame for printing.
  void RenderPagesForPrint(WebKit::WebFrame* frame,
                           WebKit::WebNode* node);

  // Render the frame for preview.
  void RenderPagesForPreview(WebKit::WebFrame* frame);

  // Renders all the pages listed in |params| for preview.
  // On Success, Send ViewHostMsg_PagesReadyForPreview message with a
  // valid metafile data handle.
  void CreatePreviewDocument(const ViewMsg_PrintPages_Params& params,
      WebKit::WebFrame* frame);
#if defined(OS_MACOSX)
  void RenderPage(const gfx::Size& page_size, const gfx::Point& content_origin,
                  const float& scale_factor, int page_number,
                  WebKit::WebFrame* frame, printing::NativeMetafile* metafile);
#elif defined(OS_WIN)
  void RenderPage(const ViewMsg_Print_Params& params, float* scale_factor,
                  int page_number, WebKit::WebFrame* frame,
                  scoped_ptr<printing::NativeMetafile>* metafile);
#endif

#if defined(OS_MACOSX) || defined(OS_WIN)
  bool CopyMetafileDataToSharedMem(printing::NativeMetafile* metafile,
      base::SharedMemoryHandle* shared_mem_handle);
#endif

  RenderView* render_view_;
  WebKit::WebView* print_web_view_;
  scoped_ptr<ViewMsg_PrintPages_Params> print_pages_params_;
  base::Time last_cancelled_script_print_;
  int user_cancelled_scripted_print_count_;
  bool is_preview_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PrintWebViewHelper);
};

#endif  // CHROME_RENDERER_PRINT_WEB_VIEW_HELPER_H_
