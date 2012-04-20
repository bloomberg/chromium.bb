// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_PRINT_PREVIEW_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_PRINT_PREVIEW_HANDLER_H_
#pragma once

#include <string>

#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/printing/print_view_manager_observer.h"
#include "chrome/browser/ui/select_file_dialog.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "printing/print_job_constants.h"

class FilePath;
class PrintSystemTaskProxy;
class TabContentsWrapper;

namespace base {
class DictionaryValue;
class RefCountedBytes;
}

namespace printing {
struct PageSizeMargins;
class PrintBackend;
class StickySettings;
}

// The handler for Javascript messages related to the print preview dialog.
class PrintPreviewHandler : public content::WebUIMessageHandler,
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
  friend class PrintPreviewHandlerTest;
  friend class PrintSystemTaskProxy;
  FRIEND_TEST_ALL_PREFIXES(PrintPreviewHandlerTest, StickyMarginsCustom);
  FRIEND_TEST_ALL_PREFIXES(PrintPreviewHandlerTest, StickyMarginsDefault);
  FRIEND_TEST_ALL_PREFIXES(PrintPreviewHandlerTest,
                           StickyMarginsCustomThenDefault);
  FRIEND_TEST_ALL_PREFIXES(PrintPreviewHandlerTest,
                           GetLastUsedMarginSettingsCustom);
  FRIEND_TEST_ALL_PREFIXES(PrintPreviewHandlerTest,
                           GetLastUsedMarginSettingsDefault);

  TabContentsWrapper* preview_tab_wrapper() const;
  content::WebContents* preview_tab() const;

  // Gets the list of printers. |args| is unused.
  void HandleGetPrinters(const base::ListValue* args);

  // Asks the initiator renderer to generate a preview.  First element of |args|
  // is a job settings JSON string.
  void HandleGetPreview(const base::ListValue* args);

  // Gets the job settings from Web UI and initiate printing. First element of
  // |args| is a job settings JSON string.
  void HandlePrint(const base::ListValue* args);

  // Handles printing to PDF. |settings| points to a dictionary containing all
  // the print request parameters.
  void HandlePrintToPdf(const base::DictionaryValue& settings);

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

  // Callback for the signin dialog to call once signin is complete.
  static void OnSigninComplete(
      const base::WeakPtr<PrintPreviewHandler>& handler);

  // Brings up a dialog to allow the user to sign into cloud print.
  // |args| is unused.
  void HandleSignin(const base::ListValue* args);

  // Brings up a web page to allow the user to configure cloud print.
  // |args| is unused.
  void HandleManageCloudPrint(const base::ListValue* args);

  // Gathers UMA stats when the print preview tab is about to close.
  // |args| is unused.
  void HandleClosePreviewTab(const base::ListValue* args);

  // Asks the browser to show the native printer management dialog.
  // |args| is unused.
  void HandleManagePrinters(const base::ListValue* args);

  // Asks the browser to show the cloud print dialog.
  void HandlePrintWithCloudPrint();

  // Asks the browser for several settings that are needed before the first
  // preview is displayed.
  void HandleGetInitialSettings(const base::ListValue* args);

  void SendInitialSettings(
      const std::string& default_printer,
      const std::string& cloud_print_data);

  // Sends the printer capabilities to the Web UI. |settings_info| contains
  // printer capabilities information.
  void SendPrinterCapabilities(const base::DictionaryValue& settings_info);

  // Send the list of printers to the Web UI.
  void SetupPrinterList(const base::ListValue& printers);

  // Send whether cloud print integration should be enabled.
  void SendCloudPrintEnabled();

  // Send the PDF data to the cloud to print.
  void SendCloudPrintJob(const base::DictionaryValue& settings,
                         std::string print_ticket);

  // Gets the initiator tab for the print preview tab.
  TabContentsWrapper* GetInitiatorTab() const;

  // Activates the initiator tab and close the preview tab.
  void ActivateInitiatorTabAndClosePreviewTab();

  // Adds all the recorded stats taken so far to histogram counts.
  void ReportStats();

  // Clears initiator tab details for this preview tab.
  void ClearInitiatorTabDetails();

  // Posts a task to save |data| to pdf at |print_to_pdf_path_|.
  void PostPrintToPdfTask(scoped_refptr<base::RefCountedBytes> data);

  // Populates |settings| according to the current locale.
  void GetNumberFormatAndMeasurementSystem(base::DictionaryValue* settings);

  static printing::StickySettings* GetStickySettings();

  // Pointer to current print system.
  scoped_refptr<printing::PrintBackend> print_backend_;

  // The underlying dialog object.
  scoped_refptr<SelectFileDialog> select_file_dialog_;

  // A count of how many requests received to regenerate preview data.
  // Initialized to 0 then incremented and emitted to a histogram.
  int regenerate_preview_request_count_;

  // A count of how many requests received to show manage printers dialog.
  int manage_printers_dialog_request_count_;

  // Whether we have already logged a failed print preview.
  bool reported_failed_preview_;

  // Whether we have already logged the number of printers this session.
  bool has_logged_printers_count_;

  // Holds the path to the print to pdf request. It is empty if no such request
  // exists.
  scoped_ptr<FilePath> print_to_pdf_path_;

  DISALLOW_COPY_AND_ASSIGN(PrintPreviewHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_PRINT_PREVIEW_HANDLER_H_
