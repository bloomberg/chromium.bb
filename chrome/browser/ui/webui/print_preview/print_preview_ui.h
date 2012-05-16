// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_PRINT_PREVIEW_UI_H_
#define CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_PRINT_PREVIEW_UI_H_
#pragma once

#include <string>

#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/time.h"
#include "chrome/browser/printing/print_preview_data_service.h"
#include "chrome/browser/ui/webui/constrained_web_dialog_ui.h"

class PrintPreviewDataService;
class PrintPreviewHandler;
struct PrintHostMsg_DidGetPreviewPageCount_Params;

namespace base {
class RefCountedBytes;
}

namespace gfx {
class Rect;
}

namespace printing {
struct PageSizeMargins;
}

class PrintPreviewUI : public ConstrainedWebDialogUI {
 public:
  explicit PrintPreviewUI(content::WebUI* web_ui);
  virtual ~PrintPreviewUI();

  // Gets the print preview |data|. |index| is zero-based, and can be
  // |printing::COMPLETE_PREVIEW_DOCUMENT_INDEX| to get the entire preview
  // document.
  void GetPrintPreviewDataForIndex(int index,
                                   scoped_refptr<base::RefCountedBytes>* data);

  // Sets the print preview |data|. |index| is zero-based, and can be
  // |printing::COMPLETE_PREVIEW_DOCUMENT_INDEX| to set the entire preview
  // document.
  void SetPrintPreviewDataForIndex(int index,
                                   const base::RefCountedBytes* data);

  // Clear the existing print preview data.
  void ClearAllPreviewData();

  // Returns the available draft page count.
  int GetAvailableDraftPageCount();

  // Setters
  void SetInitiatorTabURLAndTitle(const std::string& initiator_url,
                                  const string16& initiator_tab_title);

  string16 initiator_tab_title() { return initiator_tab_title_; }

  bool source_is_modifiable() { return source_is_modifiable_; }

  // Set |source_is_modifiable_| for |print_preview_tab|'s PrintPreviewUI.
  static void SetSourceIsModifiable(TabContentsWrapper* print_preview_tab,
                                    bool source_is_modifiable);

  // Determines whether to cancel a print preview request based on
  // |preview_ui_addr| and |request_id|.
  // Can be called from any thread.
  static void GetCurrentPrintPreviewStatus(const std::string& preview_ui_addr,
                                           int request_id,
                                           bool* cancel);

  // Returns a string to uniquely identify this PrintPreviewUI.
  std::string GetPrintPreviewUIAddress() const;

  // Notifies the Web UI of a print preview request with |request_id|.
  void OnPrintPreviewRequest(int request_id);

  // Notifies the Web UI to show the system dialog.
  void OnShowSystemDialog();

  // Notifies the Web UI about the page count of the request preview.
  void OnDidGetPreviewPageCount(
      const PrintHostMsg_DidGetPreviewPageCount_Params& params);

  // Notifies the Web UI of the default page layout according to the currently
  // selected printer and page size.
  void OnDidGetDefaultPageLayout(
      const printing::PageSizeMargins& page_layout,
      const gfx::Rect& printable_area,
      bool has_custom_page_size_style);

  // Notifies the Web UI that the 0-based page |page_number| has been rendered.
  // |preview_request_id| indicates wich request resulted in this response.
  void OnDidPreviewPage(int page_number, int preview_request_id);

  // Notifies the Web UI renderer that preview data is available.
  // |expected_pages_count| specifies the total number of pages.
  // |preview_request_id| indicates which request resulted in this response.
  void OnPreviewDataIsAvailable(int expected_pages_count,
                                int preview_request_id);

  // Notifies the Web UI renderer to reuse the preview data.
  // |preview_request_id| indicates which request resulted in this response.
  void OnReusePreviewData(int preview_request_id);

  // Notifies the Web UI that preview tab is destroyed. This is the last chance
  // to communicate with the source tab before the association is erased.
  void OnTabDestroyed();

  // Notifies the Web UI that the print preview failed to render.
  void OnPrintPreviewFailed();

  // Notified the Web UI that this print preview tab's RenderProcess has been
  // closed, which may occur for several reasons, e.g. tab closure or crash.
  void OnPrintPreviewTabClosed();

  // Notifies the Web UI that initiator tab is closed, so we can disable all the
  // controls that need the initiator tab for generating the preview data.
  void OnInitiatorTabClosed();

  // Notifies the Web UI renderer that file selection has been cancelled.
  void OnFileSelectionCancelled();

  // Notifies the Web UI that the printer is unavailable or its settings are
  // invalid.
  void OnInvalidPrinterSettings();

  // Notifies the Web UI to cancel the pending preview request.
  void OnCancelPendingPreviewRequest();

  // Hides the print preview tab.
  void OnHidePreviewTab();

  // Closes the print preview tab.
  void OnClosePrintPreviewTab();

  // Reload the printers list.
  void OnReloadPrintersList();

  // Notifies the WebUI that the pdf print scaling option is disabled by
  // default.
  void OnPrintPreviewScalingDisabled();

 private:
  friend class PrintPreviewHandlerTest;
  FRIEND_TEST_ALL_PREFIXES(PrintPreviewHandlerTest, StickyMarginsCustom);
  FRIEND_TEST_ALL_PREFIXES(PrintPreviewHandlerTest, StickyMarginsDefault);
  FRIEND_TEST_ALL_PREFIXES(PrintPreviewHandlerTest,
                           StickyMarginsCustomThenDefault);
  FRIEND_TEST_ALL_PREFIXES(PrintPreviewHandlerTest,
                           GetLastUsedMarginSettingsCustom);
  FRIEND_TEST_ALL_PREFIXES(PrintPreviewHandlerTest,
                           GetLastUsedMarginSettingsDefault);
  FRIEND_TEST_ALL_PREFIXES(PrintPreviewTabControllerUnitTest,
                           TitleAfterReload);

  // Returns the Singleton instance of the PrintPreviewDataService.
  PrintPreviewDataService* print_preview_data_service();

  base::TimeTicks initial_preview_start_time_;

  // Store the PrintPreviewUI address string.
  std::string preview_ui_addr_str_;

  // Weak pointer to the WebUI handler.
  PrintPreviewHandler* handler_;

  // Store the |initiator_url| in order to display an accurate error message
  // when the initiator tab is closed/crashed.
  std::string initiator_url_;

  // Indicates whether the source document can be modified.
  bool source_is_modifiable_;

  // Store the initiator tab title, used for populating the print preview tab
  // title.
  string16 initiator_tab_title_;

  // Keeps track of whether OnClosePrintPreviewTab() has been called or not.
  bool tab_closed_;

  DISALLOW_COPY_AND_ASSIGN(PrintPreviewUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_PRINT_PREVIEW_UI_H_
