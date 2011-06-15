// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_HANDLER_H_
#pragma once

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/shell_dialogs.h"
#include "content/browser/webui/web_ui.h"

class FilePath;
class FundamentalValue;
class PrintSystemTaskProxy;
class StringValue;

namespace printing {
class PrintBackend;
}

// The handler for Javascript messages related to the "print preview" dialog.
class PrintPreviewHandler : public WebUIMessageHandler,
                            public base::SupportsWeakPtr<PrintPreviewHandler>,
                            public SelectFileDialog::Listener {
 public:
  PrintPreviewHandler();
  virtual ~PrintPreviewHandler();

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages();

  // SelectFileDialog::Listener implementation.
  virtual void FileSelected(const FilePath& path, int index, void* params);
  virtual void FileSelectionCanceled(void* params);

  // Displays a modal dialog, prompting the user to select a file.
  void SelectFile(const FilePath& default_path);

 private:
  friend class PrintSystemTaskProxy;

  TabContents* preview_tab();

  // Get the default printer. |args| is unused.
  void HandleGetDefaultPrinter(const ListValue* args);

  // Get the list of printers. |args| is unused.
  void HandleGetPrinters(const ListValue* args);

  // Ask the initiator renderer to generate a preview.
  // First element of |args| is a job settings JSON string.
  void HandleGetPreview(const ListValue* args);

  // Get the job settings from Web UI and initiate printing.
  // First element of |args| is a job settings JSON string.
  void HandlePrint(const ListValue* args);

  // Handles the request to hide the preview tab for printing.
  // |args| is unused.
  void HandleHidePreview(const ListValue* args);

  // Handles the request to cancel the pending print request.
  // |args| is unused.
  void HandleCancelPendingPrintRequest(const ListValue* args);

  // Get the printer capabilities.
  // First element of |args| is the printer name.
  void HandleGetPrinterCapabilities(const ListValue* args);

  // Ask the initiator renderer to show the native print system dialog.
  // |args| is unused.
  void HandleShowSystemDialog(const ListValue* args);

  // Ask the browser to show the native printer management dialog.
  // |args| is unused.
  void HandleManagePrinters(const ListValue* args);

  // Ask the browser to close the preview tab.
  // |args| is unused.
  void HandleClosePreviewTab(const ListValue* args);

  // Send the printer capabilities to the Web UI.
  // |settings_info| contains printer capabilities information.
  void SendPrinterCapabilities(const DictionaryValue& settings_info);

  // Send the default printer to the Web UI.
  void SendDefaultPrinter(const StringValue& default_printer);

  // Send the list of printers to the Web UI.
  void SendPrinterList(const ListValue& printers);

  // Helper function to get the initiator tab for the print preview tab.
  TabContents* GetInitiatorTab();

  // Helper function to close the print preview tab.
  void ClosePrintPreviewTab();

  // Helper function to activate the initiator tab and close the preview tab.
  void ActivateInitiatorTabAndClosePreviewTab();

  // Adds all the recorded stats taken so far to histogram counts.
  void ReportStats();

  // Helper function to hide the preview tab for printing.
  void HidePreviewTab();

  // Helper function to clear initiator tab details for this preview tab.
  void ClearInitiatorTabDetails();

  // Pointer to current print system.
  scoped_refptr<printing::PrintBackend> print_backend_;

  // The underlying dialog object.
  scoped_refptr<SelectFileDialog> select_file_dialog_;

  static FilePath* last_saved_path_;
  static std::string* last_used_printer_;

  // A count of how many requests received to regenerate preview data.
  // Initialized to 0 then incremented and emitted to a histogram.
  int regenerate_preview_request_count_;

  // A count of how many requests received to show manage printers dialog.
  int manage_printers_dialog_request_count_;

  // Whether we have already logged a failed print preview.
  bool reported_failed_preview_;

  // Whether we have already logged the number of printers this session.
  bool has_logged_printers_count_;

  DISALLOW_COPY_AND_ASSIGN(PrintPreviewHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_HANDLER_H_
