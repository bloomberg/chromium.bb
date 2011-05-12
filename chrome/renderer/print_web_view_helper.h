// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_PRINT_WEB_VIEW_HELPER_H_
#define CHROME_RENDERER_PRINT_WEB_VIEW_HELPER_H_
#pragma once

#include "base/memory/scoped_ptr.h"
#include "base/shared_memory.h"
#include "base/time.h"
#include "content/renderer/render_view_observer.h"
#include "content/renderer/render_view_observer_tracker.h"
#include "printing/metafile.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrameClient.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebViewClient.h"
#include "ui/gfx/size.h"

class DictionaryValue;
struct PrintMsg_Print_Params;
struct PrintMsg_PrintPage_Params;
struct PrintMsg_PrintPages_Params;

#if defined(USE_X11)
namespace skia {
class VectorCanvas;
}
#endif

// Class that calls the Begin and End print functions on the frame and changes
// the size of the view temporarily to support full page printing..
// Do not serve any events in the time between construction and destruction of
// this class because it will cause flicker.
class PrepareFrameAndViewForPrint {
 public:
  // Prints |frame|.  If |node| is not NULL, then only that node will be
  // printed.
  PrepareFrameAndViewForPrint(const PrintMsg_Print_Params& print_params,
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
  gfx::Size prev_scroll_offset_;
  int expected_pages_count_;
  bool use_browser_overlays_;

  DISALLOW_COPY_AND_ASSIGN(PrepareFrameAndViewForPrint);
};


// PrintWebViewHelper handles most of the printing grunt work for RenderView.
// We plan on making print asynchronous and that will require copying the DOM
// of the document and creating a new WebView with the contents.
class PrintWebViewHelper : public RenderViewObserver ,
                           public RenderViewObserverTracker<PrintWebViewHelper>,
                           public WebKit::WebViewClient,
                           public WebKit::WebFrameClient {
 public:
  explicit PrintWebViewHelper(RenderView* render_view);
  virtual ~PrintWebViewHelper();

 protected:
  // WebKit::WebViewClient override:
  virtual void didStopLoading();

 private:
  FRIEND_TEST_ALL_PREFIXES(PrintWebViewHelperTest,
                           BlockScriptInitiatedPrinting);
  FRIEND_TEST_ALL_PREFIXES(PrintWebViewHelperTest, OnPrintPages);
  FRIEND_TEST_ALL_PREFIXES(PrintWebViewHelperPreviewTest, OnPrintPreview);
  FRIEND_TEST_ALL_PREFIXES(PrintWebViewHelperPreviewTest, OnPrintPreviewFail);
  FRIEND_TEST_ALL_PREFIXES(PrintWebViewHelperPreviewTest,
                           OnPrintForPrintPreview);
  FRIEND_TEST_ALL_PREFIXES(PrintWebViewHelperPreviewTest,
                           OnPrintForPrintPreviewFail);

#if defined(OS_WIN) || defined(OS_MACOSX)
  FRIEND_TEST_ALL_PREFIXES(PrintWebViewHelperTest, PrintLayoutTest);
  FRIEND_TEST_ALL_PREFIXES(PrintWebViewHelperTest, PrintWithIframe);
#endif  // defined(OS_WIN) || defined(OS_MACOSX)

  // RenderViewObserver implementation.
  virtual bool OnMessageReceived(const IPC::Message& message);
  virtual void PrintPage(WebKit::WebFrame* frame);

  // Message handlers ---------------------------------------------------------

  // Print the document.
  void OnPrintPages();

  // Generate a print preview using |settings|.
  void OnPrintPreview(const DictionaryValue& settings);

  // Print / preview the node under the context menu.
  void OnPrintNodeUnderContextMenu();

  // Print the pages for print preview. Do not display the native print dialog
  // for user settings. |job_settings| has new print job settings values.
  void OnPrintForPrintPreview(const DictionaryValue& job_settings);

  void OnPrintingDone(bool success);

  // Main printing code -------------------------------------------------------

  void Print(WebKit::WebFrame* frame, WebKit::WebNode* node);

  void PrintPreview(WebKit::WebFrame* frame,
                    WebKit::WebNode* node,
                    const DictionaryValue& settings);

  enum PrintingResult {
    OK,
    FAIL_PRINT,
    FAIL_PREVIEW,
  };

  // Notification when printing is done - signal teardown/free resources.
  void DidFinishPrinting(PrintingResult result);

  // Print Settings -----------------------------------------------------------

  // Initialize print page settings with default settings.
  bool InitPrintSettings(WebKit::WebFrame* frame,
                         WebKit::WebNode* node);

  // Update the current print settings with new |job_settings|. |job_settings|
  // dictionary contains print job details such as printer name, number of
  // copies, page range, etc.
  bool UpdatePrintSettings(const DictionaryValue& job_settings);

  // Get final print settings from the user.
  // Return false if the user cancels or on error.
  bool GetPrintSettingsFromUser(WebKit::WebFrame* frame,
                                int expected_pages_count,
                                bool use_browser_overlays);

  // Page Printing / Rendering ------------------------------------------------

  // Prints all the pages listed in |params|.
  // It will implicitly revert the document to display CSS media type.
  bool PrintPages(const PrintMsg_PrintPages_Params& params,
                  WebKit::WebFrame* frame,
                  WebKit::WebNode* node);

  // Prints the page listed in |params|.
#if defined(USE_X11)
  void PrintPageInternal(const PrintMsg_PrintPage_Params& params,
                         const gfx::Size& canvas_size,
                         WebKit::WebFrame* frame,
                         printing::Metafile* metafile,
                         scoped_ptr<skia::VectorCanvas>* canvas);
#else
  void PrintPageInternal(const PrintMsg_PrintPage_Params& params,
                         const gfx::Size& canvas_size,
                         WebKit::WebFrame* frame);
#endif

  // Render the frame for printing.
  bool RenderPagesForPrint(WebKit::WebFrame* frame, WebKit::WebNode* node);

  // Render the frame for preview.
  bool RenderPagesForPreview(WebKit::WebFrame* frame, WebKit::WebNode* node);

  // Renders all the pages listed in |params| for preview.
  // On success, Send PrintHostMsg_PagesReadyForPreview message with a
  // valid metafile data handle.
  bool CreatePreviewDocument(const PrintMsg_PrintPages_Params& params,
                             WebKit::WebFrame* frame,
                             WebKit::WebNode* node);

  // Platform specific helper function for rendering page(s) to |metafile|.
#if defined(OS_WIN)
  void RenderPage(const PrintMsg_Print_Params& params, float* scale_factor,
                  int page_number, bool is_preview, WebKit::WebFrame* frame,
                  scoped_ptr<printing::Metafile>* metafile);
#elif defined(OS_MACOSX)
  void RenderPage(const gfx::Size& page_size, const gfx::Rect& content_area,
                  const float& scale_factor, int page_number,
                  WebKit::WebFrame* frame, printing::Metafile* metafile);
#elif defined(OS_POSIX)
  bool RenderPages(const PrintMsg_PrintPages_Params& params,
                   WebKit::WebFrame* frame,
                   WebKit::WebNode* node,
                   bool send_expected_page_count,
                   int* page_count,
                   printing::Metafile* metafile);
#endif  // defined(OS_WIN)

  // Helper methods -----------------------------------------------------------

  bool CopyAndPrint(WebKit::WebFrame* web_frame);

  bool CopyMetafileDataToSharedMem(printing::Metafile* metafile,
                                   base::SharedMemoryHandle* shared_mem_handle);

  void GetPageSizeAndMarginsInPoints(
      WebKit::WebFrame* frame,
      int page_index,
      const PrintMsg_Print_Params& default_params,
      double* content_width_in_points,
      double* content_height_in_points,
      double* margin_top_in_points,
      double* margin_right_in_points,
      double* margin_bottom_in_points,
      double* margin_left_in_points);

  bool IsModifiable(WebKit::WebFrame* frame, WebKit::WebNode* node);

  void UpdatePrintableSizeInPrintParameters(WebKit::WebFrame* frame,
                                            WebKit::WebNode* node,
                                            PrintMsg_Print_Params* params);

  bool GetPrintFrame(WebKit::WebFrame** frame);

  // Script Initiated Printing ------------------------------------------------

  // Returns true if script initiated printing occurs too often.
  bool IsScriptInitiatedPrintTooFrequent(WebKit::WebFrame* frame);

  // Reset the counter for script initiated printing.
  // Scripted printing will be allowed to continue.
  void ResetScriptedPrintCount();

  // Increment the counter for script initiated printing.
  // Scripted printing will be blocked for a limited amount of time.
  void IncrementScriptedPrintCount();

  WebKit::WebView* print_web_view_;

  // The frame to print for script initiated print preview.
  WebKit::WebFrame* script_initiated_preview_frame_;

  // The node under the context menu to print preview.
  scoped_ptr<WebKit::WebNode> context_menu_preview_node_;

  scoped_ptr<PrintMsg_PrintPages_Params> print_pages_params_;
  bool is_preview_;

  // Used for scripted initiated printing blocking.
  base::Time last_cancelled_script_print_;
  int user_cancelled_scripted_print_count_;

  // Let the browser process know of a printing failure. Only set to false when
  // the failure came from the browser in the first place.
  bool notify_browser_of_print_failure_;

  DISALLOW_COPY_AND_ASSIGN(PrintWebViewHelper);
};

#endif  // CHROME_RENDERER_PRINT_WEB_VIEW_HELPER_H_
