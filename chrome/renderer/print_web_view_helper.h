// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_PRINT_WEB_VIEW_HELPER_H_
#define CHROME_RENDERER_PRINT_WEB_VIEW_HELPER_H_
#pragma once

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/shared_memory.h"
#include "base/time.h"
#include "content/public/renderer/render_view_observer.h"
#include "content/public/renderer/render_view_observer_tracker.h"
#include "printing/metafile.h"
#include "printing/metafile_impl.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCanvas.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrameClient.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebNode.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebViewClient.h"
#include "ui/gfx/size.h"

struct PrintMsg_Print_Params;
struct PrintMsg_PrintPage_Params;
struct PrintMsg_PrintPages_Params;

namespace base {
class DictionaryValue;
}
namespace printing {
struct PageSizeMargins;
}

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
                              const WebKit::WebNode& node);
  ~PrepareFrameAndViewForPrint();

  void UpdatePrintParams(const PrintMsg_Print_Params& print_params);

  int GetExpectedPageCount() const {
    return expected_pages_count_;
  }

  bool ShouldUseBrowserOverlays() const {
    return use_browser_overlays_;
  }

  const gfx::Size& GetPrintCanvasSize() const {
    return print_canvas_size_;
  }

  void FinishPrinting();

 private:
  void StartPrinting(const gfx::Size& print_params);

  WebKit::WebFrame* frame_;
  WebKit::WebNode node_to_print_;
  WebKit::WebView* web_view_;
  gfx::Size print_canvas_size_;
  gfx::Size prev_view_size_;
  gfx::Size prev_scroll_offset_;
  int dpi_;
  int expected_pages_count_;
  bool use_browser_overlays_;
  bool finished_;

  DISALLOW_COPY_AND_ASSIGN(PrepareFrameAndViewForPrint);
};

// PrintWebViewHelper handles most of the printing grunt work for RenderView.
// We plan on making print asynchronous and that will require copying the DOM
// of the document and creating a new WebView with the contents.
class PrintWebViewHelper
    : public content::RenderViewObserver,
      public content::RenderViewObserverTracker<PrintWebViewHelper>,
      public WebKit::WebViewClient,
      public WebKit::WebFrameClient {
 public:
  explicit PrintWebViewHelper(content::RenderView* render_view);
  virtual ~PrintWebViewHelper();

 protected:
  // WebKit::WebViewClient override:
  virtual void didStopLoading();

 private:
  friend class PrintWebViewHelperTestBase;
  FRIEND_TEST_ALL_PREFIXES(PrintWebViewHelperTest,
                           BlockScriptInitiatedPrinting);
  FRIEND_TEST_ALL_PREFIXES(PrintWebViewHelperTest, OnPrintPages);

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

  // Print the document with the print preview frame/node.
  void OnPrintForSystemDialog();

  // Initiate print preview.
  void OnInitiatePrintPreview();

  // Start the process of generating a print preview using |settings|.
  void OnPrintPreview(const base::DictionaryValue& settings);

  // Initialize the print preview document.
  bool CreatePreviewDocument();

  // Renders a print preview page. |page_number| is 0-based.
  // Returns true if print preview should continue, false on failure.
  bool RenderPreviewPage(int page_number);

  // Finalize the print ready preview document.
  bool FinalizePrintReadyDocument();

  // Print / preview the node under the context menu.
  void OnPrintNodeUnderContextMenu();

  // Print the pages for print preview. Do not display the native print dialog
  // for user settings. |job_settings| has new print job settings values.
  void OnPrintForPrintPreview(const base::DictionaryValue& job_settings);

  void OnPrintingDone(bool success);

  // Main printing code -------------------------------------------------------

  void Print(WebKit::WebFrame* frame, const WebKit::WebNode& node);

  enum PrintingResult {
    OK,
    FAIL_PRINT,
    FAIL_PREVIEW,
  };

  // Notification when printing is done - signal tear-down/free resources.
  void DidFinishPrinting(PrintingResult result);

  // Print Settings -----------------------------------------------------------

  // Initialize print page settings with default settings.
  // Used only for native printing workflow.
  bool InitPrintSettings(WebKit::WebFrame* frame, const WebKit::WebNode& node);

  // Initialize print page settings with default settings and prepare the frame
  // for print. A new PrepareFrameAndViewForPrint is created to fulfill the
  // request and is filled into the |prepare| argument.
  // Used only for native printing workflow.
  bool InitPrintSettingsAndPrepareFrame(
      WebKit::WebFrame* frame,
      const WebKit::WebNode& node,
      scoped_ptr<PrepareFrameAndViewForPrint>* prepare);

  // Update the current print settings with new |job_settings|. |job_settings|
  // dictionary contains print job details such as printer name, number of
  // copies, page range, etc.
  bool UpdatePrintSettings(WebKit::WebFrame* frame,
                           const WebKit::WebNode& node,
                           const base::DictionaryValue& passed_job_settings,
                           bool print_for_preview);

  // Get final print settings from the user.
  // Return false if the user cancels or on error.
  bool GetPrintSettingsFromUser(WebKit::WebFrame* frame,
                                const WebKit::WebNode& node,
                                int expected_pages_count,
                                bool use_browser_overlays);

  // Page Printing / Rendering ------------------------------------------------

  // Prints all the pages listed in |params|.
  // It will implicitly revert the document to display CSS media type.
  bool PrintPages(const PrintMsg_PrintPages_Params& params,
                  WebKit::WebFrame* frame,
                  const WebKit::WebNode& node);

  // Prints the page listed in |params|.
#if defined(USE_X11)
  void PrintPageInternal(const PrintMsg_PrintPage_Params& params,
                         const gfx::Size& canvas_size,
                         WebKit::WebFrame* frame,
                         printing::Metafile* metafile);
#else
  void PrintPageInternal(const PrintMsg_PrintPage_Params& params,
                         const gfx::Size& canvas_size,
                         WebKit::WebFrame* frame);
#endif

  // Render the frame for printing.
  bool RenderPagesForPrint(WebKit::WebFrame* frame,
                           const WebKit::WebNode& node);

  // Platform specific helper function for rendering page(s) to |metafile|.
#if defined(OS_WIN)
  // Because of mixed support for alpha channels on printers, this method may
  // need to create a new metafile.  The result may be either the passed
  // |metafile| or a new one.  In either case, the caller owns both |metafile|
  // and the result.
  printing::Metafile* RenderPage(const PrintMsg_Print_Params& params,
                                 float* scale_factor, int page_number,
                                 bool is_preview, WebKit::WebFrame* frame,
                                 printing::Metafile* metafile);
#elif defined(OS_MACOSX)
  void RenderPage(const gfx::Size& page_size, const gfx::Rect& content_area,
                  const float& scale_factor, int page_number,
                  WebKit::WebFrame* frame, bool is_preview,
                  printing::Metafile* metafile);
#elif defined(OS_POSIX)
  bool RenderPages(const PrintMsg_PrintPages_Params& params,
                   WebKit::WebFrame* frame, const WebKit::WebNode& node,
                   int* page_count, PrepareFrameAndViewForPrint* prepare,
                   printing::Metafile* metafile);
#endif  // defined(OS_WIN)

  // Helper methods -----------------------------------------------------------

  bool CopyAndPrint(WebKit::WebFrame* web_frame);

  bool CopyMetafileDataToSharedMem(printing::Metafile* metafile,
                                   base::SharedMemoryHandle* shared_mem_handle);

  static void GetPageSizeAndMarginsInPoints(
      WebKit::WebFrame* frame,
      int page_index,
      const PrintMsg_Print_Params& default_params,
      printing::PageSizeMargins* page_layout_in_points);

  static void UpdatePrintableSizeInPrintParameters(
      WebKit::WebFrame* frame,
      const WebKit::WebNode& node,
      PrepareFrameAndViewForPrint* prepare,
      PrintMsg_Print_Params* params);

  // Given the |device| and |canvas| to draw on, prints the appropriate headers
  // and footers using strings from |header_footer_info| on to the canvas.
  static void PrintHeaderAndFooter(
      WebKit::WebCanvas* canvas,
      int page_number,
      int total_pages,
      float webkit_scale_factor,
      const printing::PageSizeMargins& page_layout_in_points,
      const base::DictionaryValue& header_footer_info);

  bool GetPrintFrame(WebKit::WebFrame** frame);

  // This reports the current time - |start_time| as the time to render a page.
  void ReportPreviewPageRenderTime(base::TimeTicks start_time);

  // Script Initiated Printing ------------------------------------------------

  // Returns true if script initiated printing occurs too often.
  bool IsScriptInitiatedPrintTooFrequent(WebKit::WebFrame* frame);

  // Reset the counter for script initiated printing.
  // Scripted printing will be allowed to continue.
  void ResetScriptedPrintCount();

  // Increment the counter for script initiated printing.
  // Scripted printing will be blocked for a limited amount of time.
  void IncrementScriptedPrintCount();

  // Displays the print job error message to the user.
  void DisplayPrintJobError();

  void RequestPrintPreview();

  // Checks whether print preview should continue or not.
  // Returns true if cancelling, false if continuing.
  bool CheckForCancel();

  // Notifies the browser a print preview page has been rendered.
  // |page_number| is 0-based.
  // For a valid |page_number| with modifiable content,
  // |metafile| is the rendered page. Otherwise |metafile| is NULL.
  // Returns true if print preview should continue, false on failure.
  bool PreviewPageRendered(int page_number, printing::Metafile* metafile);

  WebKit::WebView* print_web_view_;

  scoped_ptr<PrintMsg_PrintPages_Params> print_pages_params_;
  bool is_preview_enabled_;
  bool is_print_ready_metafile_sent_;

  // Used for scripted initiated printing blocking.
  base::Time last_cancelled_script_print_;
  int user_cancelled_scripted_print_count_;

  // Let the browser process know of a printing failure. Only set to false when
  // the failure came from the browser in the first place.
  bool notify_browser_of_print_failure_;

  scoped_ptr<PrintMsg_PrintPages_Params> old_print_pages_params_;

  // Strings generated by the browser process to be printed as headers and
  // footers if requested by the user.
  scoped_ptr<base::DictionaryValue> header_footer_info_;

  enum PrintPreviewErrorBuckets {
    PREVIEW_ERROR_NONE,  // Always first.
    PREVIEW_ERROR_BAD_SETTING,
    PREVIEW_ERROR_METAFILE_COPY_FAILED,
    PREVIEW_ERROR_METAFILE_INIT_FAILED,
    PREVIEW_ERROR_ZERO_PAGES,
    PREVIEW_ERROR_MAC_DRAFT_METAFILE_INIT_FAILED,
    PREVIEW_ERROR_PAGE_RENDERED_WITHOUT_METAFILE,
    PREVIEW_ERROR_UPDATING_PRINT_SETTINGS,
    PREVIEW_ERROR_INVALID_PRINTER_SETTINGS,
    PREVIEW_ERROR_LAST_ENUM  // Always last.
  };

  // Keeps track of the state of print preview between messages.
  class PrintPreviewContext {
   public:
    PrintPreviewContext();
    ~PrintPreviewContext();

    // Initializes the print preview context. Need to be called to set
    // the |web_frame| / |web_node| to generate the print preview for.
    void InitWithFrame(WebKit::WebFrame* web_frame);
    void InitWithNode(const WebKit::WebNode& web_node);

    // Does bookkeeping at the beginning of print preview.
    void OnPrintPreview();

    // Create the print preview document. |pages| is empty to print all pages.
    bool CreatePreviewDocument(PrintMsg_Print_Params* params,
                               const std::vector<int>& pages);

    // Called after a page gets rendered. |page_time| is how long the
    // rendering took.
    void RenderedPreviewPage(const base::TimeDelta& page_time);

    // Updates the print preview context when the required pages are rendered.
    void AllPagesRendered();

    // Finalizes the print ready preview document.
    void FinalizePrintReadyDocument();

    // Cleanup after print preview finishes.
    void Finished();

    // Cleanup after print preview fails.
    void Failed(bool report_error);

    // Helper functions
    int GetNextPageNumber();
    bool IsRendering() const;
    bool IsModifiable() const;
    bool IsLastPageOfPrintReadyMetafile() const;
    bool IsFinalPageRendered() const;

    // Setters
    void set_generate_draft_pages(bool generate_draft_pages);
    void set_error(enum PrintPreviewErrorBuckets error);

    // Getters
    WebKit::WebFrame* frame() const;
    const WebKit::WebNode& node() const;
    int total_page_count() const;
    bool generate_draft_pages();
    printing::PreviewMetafile* metafile() const;
    const PrintMsg_Print_Params& print_params() const;
    const gfx::Size& GetPrintCanvasSize() const;
    int last_error() const;

   private:
    enum State {
      UNINITIALIZED,  // Not ready to render.
      INITIALIZED,    // Ready to render.
      RENDERING,      // Rendering.
      DONE            // Finished rendering.
    };

    // Reset some of the internal rendering context.
    void ClearContext();

    // Specifies what to render for print preview.
    WebKit::WebFrame* frame_;
    WebKit::WebNode node_;

    scoped_ptr<PrepareFrameAndViewForPrint> prep_frame_view_;
    scoped_ptr<printing::PreviewMetafile> metafile_;
    scoped_ptr<PrintMsg_Print_Params> print_params_;

    // Total page count in the renderer.
    int total_page_count_;

    // The current page to render.
    int current_page_index_;

    // List of page indices that need to be rendered.
    std::vector<int> pages_to_render_;

    // True, when draft pages needs to be generated.
    bool generate_draft_pages_;

    // Specifies the total number of pages in the print ready metafile.
    int print_ready_metafile_page_count_;

    base::TimeDelta document_render_time_;
    base::TimeTicks begin_time_;

    enum PrintPreviewErrorBuckets error_;

    State state_;
  };

  PrintPreviewContext print_preview_context_;

  DISALLOW_COPY_AND_ASSIGN(PrintWebViewHelper);
};

#endif  // CHROME_RENDERER_PRINT_WEB_VIEW_HELPER_H_
