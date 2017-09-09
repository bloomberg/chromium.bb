// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_PRINT_PREVIEW_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_PRINT_PREVIEW_HANDLER_H_

#include <memory>
#include <queue>
#include <string>

#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "chrome/common/features.h"
#include "components/signin/core/browser/gaia_cookie_manager_service.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "printing/backend/print_backend.h"
#include "printing/features/features.h"
#include "ui/shell_dialogs/select_file_dialog.h"

class PrinterHandler;
class PrintPreviewUI;

namespace base {
class DictionaryValue;
class RefCountedBytes;
}

namespace content {
class WebContents;
}

namespace printing {

// Must match print_preview.PrinterType in
// chrome/browser/resources/print_preview/native_layer.js
enum PrinterType { kPrivetPrinter, kExtensionPrinter };

class PrinterBackendProxy;
}

// The handler for Javascript messages related to the print preview dialog.
class PrintPreviewHandler
    : public content::WebUIMessageHandler,
      public ui::SelectFileDialog::Listener,
      public GaiaCookieManagerService::Observer {
 public:
  PrintPreviewHandler();
  ~PrintPreviewHandler() override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

  // SelectFileDialog::Listener implementation.
  void FileSelected(const base::FilePath& path,
                    int index,
                    void* params) override;
  void FileSelectionCanceled(void* params) override;

  // GaiaCookieManagerService::Observer implementation.
  void OnAddAccountToCookieCompleted(
      const std::string& account_id,
      const GoogleServiceAuthError& error) override;

  // Called when print preview failed.
  void OnPrintPreviewFailed();

  // Called when print preview is cancelled due to a new request.
  void OnPrintPreviewCancelled();

  // Called when printer settings were invalid.
  void OnInvalidPrinterSettings();

  // Called when print preview is ready.
  void OnPrintPreviewReady(int preview_uid, int request_id);

  // Called when a print request is cancelled due to its initiator closing.
  void OnPrintRequestCancelled();

  // Send the print preset options from the document.
  void SendPrintPresetOptions(bool disable_scaling, int copies, int duplex);

  // Send the print preview page count and fit to page scaling
  void SendPageCountReady(int page_count,
                          int request_id,
                          int fit_to_page_scaling);

  // Send the default page layout
  void SendPageLayoutReady(const base::DictionaryValue& layout,
                           bool has_custom_page_size_style);

  // Notify the WebUI that the page preview is ready.
  void SendPagePreviewReady(int page_index,
                            int preview_uid,
                            int preview_response_id);

#if BUILDFLAG(ENABLE_BASIC_PRINTING)
  // Called when the user press ctrl+shift+p to display the native system
  // dialog.
  void ShowSystemDialog();
#endif  // BUILDFLAG(ENABLE_BASIC_PRINTING)

  int regenerate_preview_request_count() const {
    return regenerate_preview_request_count_;
  }

  // Shuts down the initiator renderer. Called when a bad IPC message is
  // received.
  void BadMessageReceived();

  // Sets |pdf_file_saved_closure_| to |closure|.
  void SetPdfSavedClosureForTesting(const base::Closure& closure);

  // Fires the 'enable-manipulate-settings-for-test' WebUI event.
  void SendEnableManipulateSettingsForTest();

  // Fires the 'manipulate-settings-for-test' WebUI event with |settings|.
  void SendManipulateSettingsForTest(const base::DictionaryValue& settings);

 protected:
  // If |prompt_user| is true, starts a task to create the default Save As PDF
  // directory if needed. OnDirectoryCreated() will be called when it
  // finishes to open the modal dialog and prompt the user. Otherwise, just
  // accept |default_path| and uniquify it.
  // Protected so unit tests can access.
  virtual void SelectFile(const base::FilePath& default_path, bool prompt_user);

  // Handles printing to PDF. Protected to expose to unit tests.
  void PrintToPdf();

  // The underlying dialog object. Protected to expose to unit tests.
  scoped_refptr<ui::SelectFileDialog> select_file_dialog_;

 private:
  friend class PrintPreviewPdfGeneratedBrowserTest;
  FRIEND_TEST_ALL_PREFIXES(PrintPreviewPdfGeneratedBrowserTest,
                           MANUAL_DummyTest);
  class AccessTokenService;

  content::WebContents* preview_web_contents() const;

  PrintPreviewUI* print_preview_ui() const;

  printing::PrinterBackendProxy* printer_backend_proxy();

  // Gets the list of printers. |args| is unused.
  void HandleGetPrinters(const base::ListValue* args);

  // Starts getting all extension managed or local privet printers.
  // |args| contains the type of printers and the Javascript callback.
  void HandleGetExtensionOrPrivetPrinters(const base::ListValue* args);

  // Grants an extension access to a provisional printer.  First element of
  // |args| is the provisional printer ID.
  void HandleGrantExtensionPrinterAccess(const base::ListValue* args);

  // Asks the initiator renderer to generate a preview.  First element of |args|
  // is a job settings JSON string.
  void HandleGetPreview(const base::ListValue* args);

  // Gets the job settings from Web UI and initiate printing. First element of
  // |args| is a job settings JSON string.
  void HandlePrint(const base::ListValue* args);

  // Handles the request to hide the preview dialog for printing.
  // |args| is unused.
  void HandleHidePreview(const base::ListValue* args);

  // Handles the request to cancel the pending print request. |args| is unused.
  void HandleCancelPendingPrintRequest(const base::ListValue* args);

  // Handles a request to store data that the web ui wishes to persist.
  // First element of |args| is the data to persist.
  void HandleSaveAppState(const base::ListValue* args);

  // Gets the printer capabilities. First element of |args| is the printer name.
  void HandleGetPrinterCapabilities(const base::ListValue* args);

  // Performs printer setup. First element of |args| is the printer name.
  void HandlePrinterSetup(const base::ListValue* args);

#if BUILDFLAG(ENABLE_BASIC_PRINT_DIALOG)
  // Asks the initiator renderer to show the native print system dialog. |args|
  // is unused.
  void HandleShowSystemDialog(const base::ListValue* args);
#endif

  // Callback for the signin dialog to call once signin is complete.
  void OnSigninComplete(const std::string& callback_id);

  // Brings up a dialog to allow the user to sign into cloud print.
  // |args| is unused.
  void HandleSignin(const base::ListValue* args);

  // Generates new token and sends back to UI.
  void HandleGetAccessToken(const base::ListValue* args);

  // Brings up a web page to allow the user to configure cloud print.
  // |args| is unused.
  void HandleManageCloudPrint(const base::ListValue* args);

  // Gathers UMA stats when the print preview dialog is about to close.
  // |args| is unused.
  void HandleClosePreviewDialog(const base::ListValue* args);

  // Asks the browser to show the native printer management dialog.
  // |args| is unused.
  void HandleManagePrinters(const base::ListValue* args);

  // Asks the browser for several settings that are needed before the first
  // preview is displayed.
  void HandleGetInitialSettings(const base::ListValue* args);

  // Reports histogram data for a print preview UI action. |args| should consist
  // of two elements: the bucket name, and the bucket event.
  void HandleReportUiEvent(const base::ListValue* args);

  // Forces the opening of a new tab. |args| should consist of one element: the
  // URL to set the new tab to.
  //
  // NOTE: This is needed to open register promo for Cloud Print as a new tab.
  // Javascript's "window.open" opens a new window popup (since initiated from
  // async HTTP request) and worse yet, on Windows and Chrome OS, the opened
  // window opens behind the initiator window.
  void HandleForceOpenNewTab(const base::ListValue* args);

  // Requests a privet or extension managed printer's capabilities.
  // |args| contains the ID and type of the printer whose capabilities are
  // requested.
  void HandleGetExtensionOrPrivetPrinterCapabilities(
      const base::ListValue* args);

  void SendInitialSettings(const std::string& callback_id,
                           const std::string& default_printer);

  // Send OAuth2 access token.
  void SendAccessToken(const std::string& callback_id,
                       const std::string& access_token);

  // Send message indicating a request for token was already in progress.
  void SendRequestInProgress(const std::string& callback_id);

  // Sends the printer capabilities to the Web UI. |settings_info| contains
  // printer capabilities information. If |settings_info| is empty, sends
  // error notification to the Web UI instead.
  void SendPrinterCapabilities(
      const std::string& callback_id,
      std::unique_ptr<base::DictionaryValue> settings_info);

  // Send the result of performing printer setup. |settings_info| contains
  // printer capabilities.
  void SendPrinterSetup(const std::string& callback_id,
                        const std::string& printer_name,
                        std::unique_ptr<base::DictionaryValue> settings_info);

  // Send the list of printers to the Web UI.
  void SetupPrinterList(const std::string& callback_id,
                        const printing::PrinterList& printer_list);

  // Send whether cloud print integration should be enabled.
  void SendCloudPrintEnabled();

  // Send the PDF data to the cloud to print.
  void SendCloudPrintJob(const std::string& callback_id,
                         const base::RefCountedBytes* data);

  // Gets the initiator for the print preview dialog.
  content::WebContents* GetInitiator() const;

  // Closes the preview dialog.
  void ClosePreviewDialog();

  // Adds all the recorded stats taken so far to histogram counts.
  void ReportStats();

  // Clears initiator details for the print preview dialog.
  void ClearInitiatorDetails();

  // Called when the directory to save to has been created. Opens a modal
  // dialog to prompt the user to select the file for Save As PDF.
  void OnDirectoryCreated(const base::FilePath& path);

  // Posts a task to save |data| to pdf at |print_to_pdf_path_|.
  void PostPrintToPdfTask();

  // Populates |settings| according to the current locale.
  void GetNumberFormatAndMeasurementSystem(base::DictionaryValue* settings);

  bool GetPreviewDataAndTitle(scoped_refptr<base::RefCountedBytes>* data,
                              base::string16* title) const;

  // Helper for getting a unique file name for SelectFile() without prompting
  // the user. Just an adaptor for FileSelected().
  void OnGotUniqueFileName(const base::FilePath& path);

#if defined(USE_CUPS)
  void SaveCUPSColorSetting(const base::DictionaryValue* settings);

  void ConvertColorSettingToCUPSColorModel(
      base::DictionaryValue* settings) const;
#endif

  PrinterHandler* GetPrinterHandler(printing::PrinterType printer_type);

  // Called when a privet printer or extension printers are detected.
  // |callback_id|: The javascript callback to call if all printers have been
  //     loaded (when |done| = true).
  // |printer_type|: The type of printers that were added.
  // |printers|: A list containing extension printers or a privet printer that
  //     has been found.
  // |done|: Whether the printers are done being loaded (all extensions
  //     have responded, or privet printer search has timed out).
  void OnGotPrintersForExtensionOrPrivet(const std::string& callback_id,
                                         printing::PrinterType printer_type,
                                         const base::ListValue& printers,
                                         bool done);

  // Called when an extension reports information requested for a provisional
  // printer.
  // |callback_id|: The javascript callback to resolve or reject.
  // |printer_info|: The data reported by the extension.
  void OnGotExtensionPrinterInfo(const std::string& callback_id,
                                 const base::DictionaryValue& printer_info);

  // Called when an extension or privet print job is completed.
  // |callback_id|: The javascript callback to run.
  // |success|: Whether the job succeeded.
  // |error|: The returned print job error. Useful for reporting a specific
  //     error.
  void OnPrintResult(const std::string& callback_id,
                     bool success,
                     const base::Value& error);

  // Register/unregister from notifications of changes done to the GAIA
  // cookie.
  void RegisterForGaiaCookieChanges();
  void UnregisterForGaiaCookieChanges();

  // A count of how many requests received to regenerate preview data.
  // Initialized to 0 then incremented and emitted to a histogram.
  int regenerate_preview_request_count_;

  // A count of how many requests received to show manage printers dialog.
  int manage_printers_dialog_request_count_;
  int manage_cloud_printers_dialog_request_count_;

  // Whether we have already logged a failed print preview.
  bool reported_failed_preview_;

  // Whether we have already logged the number of printers this session.
  bool has_logged_printers_count_;

  // Holds the path to the print to pdf request. It is empty if no such request
  // exists.
  base::FilePath print_to_pdf_path_;

  // Holds token service to get OAuth2 access tokens.
  std::unique_ptr<AccessTokenService> token_service_;

  // Pointer to cookie manager service so that print preview can listen for GAIA
  // cookie changes.
  GaiaCookieManagerService* gaia_cookie_manager_service_;

  // Handles requests for extension printers. Created lazily by calling
  // |GetPrinterHandler|.
  std::unique_ptr<PrinterHandler> extension_printer_handler_;

  // Handles requests for privet printers. Created lazily by calling
  // |GetPrinterHandler|.
  std::unique_ptr<PrinterHandler> privet_printer_handler_;

  // Notifies tests that want to know if the PDF has been saved. This doesn't
  // notify the test if it was a successful save, only that it was attempted.
  base::Closure pdf_file_saved_closure_;

  std::queue<std::string> preview_callbacks_;

  // Callback ID to be used to notify UI that PDF file selection has finished.
  std::string pdf_callback_id_;

  // Print settings to use in the local print request to send when
  // HandleHidePreview() is called.
  std::unique_ptr<base::DictionaryValue> settings_;

  // Proxy for calls to the print backend.  Lazily initialized since web_ui() is
  // not available at construction time.
  std::unique_ptr<printing::PrinterBackendProxy> printer_backend_proxy_;

  base::WeakPtrFactory<PrintPreviewHandler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PrintPreviewHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_PRINT_PREVIEW_HANDLER_H_
