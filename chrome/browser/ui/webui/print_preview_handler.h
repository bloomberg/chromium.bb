// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_HANDLER_H_
#pragma once

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/printing/print_view_manager_observer.h"
#include "chrome/browser/ui/shell_dialogs.h"
#include "content/browser/webui/web_ui.h"

class FilePath;
class PrintSystemTaskProxy;
class TabContentsWrapper;

namespace base {
class FundamentalValue;
class StringValue;
}

namespace printing {
class PrintBackend;
}

// The handler for Javascript messages related to the print preview dialog.
class PrintPreviewHandler : public WebUIMessageHandler,
                            public base::SupportsWeakPtr<PrintPreviewHandler>,
                            public SelectFileDialog::Listener,
                            public printing::PrintViewManagerObserver {
 public:
  PrintPreviewHandler();
  virtual ~PrintPreviewHandler();

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

  // SelectFileDialog::Listener implementation.
  virtual void FileSelected(const FilePath& path,
                            int index,
                            void* params) OVERRIDE;
  virtual void FileSelectionCanceled(void* params) OVERRIDE;

  // PrintViewManagerObserver implementation.
  virtual void OnPrintDialogShown() OVERRIDE;

  // Displays a modal dialog, prompting the user to select a file.
  void SelectFile(const FilePath& default_path);

  // Called when the print preview tab is destroyed. This is the last time
  // this object has access to the PrintViewManager in order to disconnect the
  // observer.
  void OnTabDestroyed();

  // Called when print preview failed.
  void OnPrintPreviewFailed();

  // Called when the user press ctrl+shift+p to display the native system
  // dialog.
  void ShowSystemDialog();

 private:
  friend class PrintSystemTaskProxy;

  TabContentsWrapper* preview_tab_wrapper() const;
  TabContents* preview_tab() const;

  // Gets the default printer. |args| is unused.
  void HandleGetDefaultPrinter(const base::ListValue* args);

  // Gets the list of printers. |args| is unused.
  void HandleGetPrinters(const base::ListValue* args);

  // Asks the initiator renderer to generate a preview.  First element of |args|
  // is a job settings JSON string.
  void HandleGetPreview(const base::ListValue* args);

  // Gets the job settings from Web UI and initiate printing.  First element of
  // |args| is a job settings JSON string.
  void HandlePrint(const base::ListValue* args);

  // Handles the request to hide the preview tab for printing. |args| is unused.
  void HandleHidePreview(const base::ListValue* args);

  // Handles the request to cancel the pending print request. |args| is unused.
  void HandleCancelPendingPrintRequest(const base::ListValue* args);

  // Handles a request to back up data about the last used cloud print
  // printer.
  // First element of |args| is the printer name.
  // Second element of |args| is the current cloud print data JSON.
  void HandleSaveLastPrinter(const base::ListValue* args);

  // Gets the printer capabilities. First element of |args| is the printer name.
  void HandleGetPrinterCapabilities(const base::ListValue* args);

  // Asks the initiator renderer to show the native print system dialog. |args|
  // is unused.
  void HandleShowSystemDialog(const base::ListValue* args);

  // Brings up a dialog to allow the user to sign into cloud print.
  // |args| is unused.
  void HandleSignin(const base::ListValue* args);

  // Brings up a web page to allow the user to configure cloud print.
  // |args| is unused.
  void HandleManageCloudPrint(const base::ListValue* args);

  // Reloads the initiator tab and closes the associated preview tab. |args| is
  // unused.
  void HandleReloadCrashedInitiatorTab(const base::ListValue* args);

  // Asks the browser to show the native printer management dialog.
  // |args| is unused.
  void HandleManagePrinters(const base::ListValue* args);

  // Asks the browser to close the preview tab. |args| is unused.
  void HandleClosePreviewTab(const base::ListValue* args);

  // Sends the printer capabilities to the Web UI. |settings_info| contains
  // printer capabilities information.
  void SendPrinterCapabilities(const base::DictionaryValue& settings_info);

  // Sends the default printer to the Web UI.
  void SendDefaultPrinter(const base::StringValue& default_printer,
                          const base::StringValue& cloud_print_data);

  // Send the list of printers to the Web UI.
  void SetupPrinterList(const base::ListValue& printers);

  // Send whether cloud print integration should be enabled.
  void SendCloudPrintEnabled();

  // Send the PDF data to the cloud to print.
  void SendCloudPrintJob(const base::DictionaryValue& settings,
                         std::string print_ticket);

  // Gets the initiator tab for the print preview tab.
  TabContentsWrapper* GetInitiatorTab() const;

  // Closes the print preview tab.
  void ClosePrintPreviewTab();

  // Activates the initiator tab and close the preview tab.
  void ActivateInitiatorTabAndClosePreviewTab();

  // Adds all the recorded stats taken so far to histogram counts.
  void ReportStats();

  // Hides the preview tab for printing.
  void HidePreviewTab();

  // Clears initiator tab details for this preview tab.
  void ClearInitiatorTabDetails();

  // Pointer to current print system.
  scoped_refptr<printing::PrintBackend> print_backend_;

  // The underlying dialog object.
  scoped_refptr<SelectFileDialog> select_file_dialog_;

  static FilePath* last_saved_path_;
  static std::string* last_used_printer_cloud_print_data_;
  static std::string* last_used_printer_name_;
  static bool last_used_color_setting_;

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
