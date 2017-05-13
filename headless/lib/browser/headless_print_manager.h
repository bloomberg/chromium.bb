// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_LIB_BROWSER_HEADLESS_PRINT_MANAGER_H_
#define HEADLESS_LIB_BROWSER_HEADLESS_PRINT_MANAGER_H_

#include <memory>
#include <string>
#include <vector>

#include "components/printing/browser/print_manager.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents_user_data.h"
#include "headless/public/headless_export.h"
#include "printing/print_settings.h"
#include "printing/printing_export.h"

struct PrintHostMsg_DidPrintPage_Params;
struct PrintHostMsg_ScriptedPrint_Params;
struct PrintMsg_PrintPages_Params;

namespace printing {

struct HeadlessPrintSettings {
  HeadlessPrintSettings()
      : landscape(false),
        display_header_footer(false),
        should_print_backgrounds(false),
        scale(1) {}

  gfx::Size paper_size_in_points;
  PageMargins margins_in_points;

  bool landscape;
  bool display_header_footer;
  bool should_print_backgrounds;
  // scale = 1 means 100%.
  double scale;

  std::string page_ranges;
};

class HeadlessPrintManager
    : public PrintManager,
      public content::WebContentsUserData<HeadlessPrintManager> {
 public:
  enum PrintResult {
    PRINT_SUCCESS,
    PRINTING_FAILED,
    INVALID_PRINTER_SETTINGS,
    INVALID_MEMORY_HANDLE,
    METAFILE_MAP_ERROR,
    UNEXPECTED_VALID_MEMORY_HANDLE,
    METAFILE_INVALID_HEADER,
    METAFILE_GET_DATA_ERROR,
    SIMULTANEOUS_PRINT_ACTIVE,
    PAGE_RANGE_SYNTAX_ERROR,
    PAGE_COUNT_EXCEEDED,
  };

  enum PageRangeStatus {
    PRINT_NO_ERROR,
    SYNTAX_ERROR,
    LIMIT_ERROR,
  };

  using GetPDFCallback = base::Callback<void(PrintResult, const std::string&)>;

  ~HeadlessPrintManager() override;

  static std::string PrintResultToString(PrintResult result);
  static std::unique_ptr<base::DictionaryValue> PDFContentsToDictionaryValue(
      const std::string& data);
  // Exported for tests.
  HEADLESS_EXPORT static PageRangeStatus PageRangeTextToPages(
      base::StringPiece page_range_text,
      int pages_count,
      std::vector<int>* pages);

  // Prints the current document immediately. Since the rendering is
  // asynchronous, the actual printing will not be completed on the return of
  // this function, and |callback| will always get called when printing
  // finishes.
  void GetPDFContents(content::RenderFrameHost* rfh,
                      const HeadlessPrintSettings& settings,
                      const GetPDFCallback& callback);

 private:
  explicit HeadlessPrintManager(content::WebContents* web_contents);
  friend class content::WebContentsUserData<HeadlessPrintManager>;

  std::unique_ptr<PrintMsg_PrintPages_Params> GetPrintParamsFromSettings(
      const HeadlessPrintSettings& settings);
  // content::WebContentsObserver implementation.
  bool OnMessageReceived(const IPC::Message& message,
                         content::RenderFrameHost* render_frame_host) override;

  // IPC Message handlers.
  void OnGetDefaultPrintSettings(IPC::Message* reply_msg);
  void OnScriptedPrint(const PrintHostMsg_ScriptedPrint_Params& params,
                       IPC::Message* reply_msg);
  void OnShowInvalidPrinterSettingsError();
  void OnPrintingFailed(int cookie) override;
  void OnDidGetPrintedPagesCount(int cookie, int number_pages) override;
  void OnDidPrintPage(const PrintHostMsg_DidPrintPage_Params& params);

  void Reset();
  void ReleaseJob(PrintResult result);

  content::RenderFrameHost* printing_rfh_;
  GetPDFCallback callback_;
  std::unique_ptr<PrintMsg_PrintPages_Params> print_params_;
  std::string page_ranges_text_;
  std::string data_;

  // Set to true when OnDidPrintPage() should be expecting the first page.
  bool expecting_first_page_;

  DISALLOW_COPY_AND_ASSIGN(HeadlessPrintManager);
};

}  // namespace printing

#endif  // HEADLESS_LIB_BROWSER_HEADLESS_PRINT_MANAGER_H_
