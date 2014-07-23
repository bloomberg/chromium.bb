// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_PRINT_PREVIEW_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_PRINT_PREVIEW_HANDLER_H_

#include <string>

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/printing/print_view_manager_observer.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "google_apis/gaia/merge_session_helper.h"
#include "ui/shell_dialogs/select_file_dialog.h"

#if defined(ENABLE_SERVICE_DISCOVERY)
#include "chrome/browser/local_discovery/privet_local_printer_lister.h"
#include "chrome/browser/local_discovery/service_discovery_shared_client.h"
#endif  // ENABLE_SERVICE_DISCOVERY

class AccountReconcilor;
class PrintSystemTaskProxy;

namespace base {
class DictionaryValue;
class RefCountedBytes;
}

namespace content {
class WebContents;
}

namespace gfx {
class Size;
}

// The handler for Javascript messages related to the print preview dialog.
class PrintPreviewHandler
    : public content::WebUIMessageHandler,
#if defined(ENABLE_SERVICE_DISCOVERY)
      public local_discovery::PrivetLocalPrinterLister::Delegate,
      public local_discovery::PrivetLocalPrintOperation::Delegate,
#endif
      public ui::SelectFileDialog::Listener,
      public printing::PrintViewManagerObserver,
      public MergeSessionHelper::Observer {
 public:
  PrintPreviewHandler();
  virtual ~PrintPreviewHandler();

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

  // SelectFileDialog::Listener implementation.
  virtual void FileSelected(const base::FilePath& path,
                            int index,
                            void* params) OVERRIDE;
  virtual void FileSelectionCanceled(void* params) OVERRIDE;

  // PrintViewManagerObserver implementation.
  virtual void OnPrintDialogShown() OVERRIDE;

  // MergeSessionHelper::Observer implementation.
  virtual void MergeSessionCompleted(
      const std::string& account_id,
      const GoogleServiceAuthError& error) OVERRIDE;

  // Displays a modal dialog, prompting the user to select a file.
  void SelectFile(const base::FilePath& default_path);

  // Called when the print preview dialog is destroyed. This is the last time
  // this object has access to the PrintViewManager in order to disconnect the
  // observer.
  void OnPrintPreviewDialogDestroyed();

  // Called when print preview failed.
  void OnPrintPreviewFailed();

  // Called when the user press ctrl+shift+p to display the native system
  // dialog.
  void ShowSystemDialog();

#if defined(ENABLE_SERVICE_DISCOVERY)
  // PrivetLocalPrinterLister::Delegate implementation.
  virtual void LocalPrinterChanged(
      bool added,
      const std::string& name,
      bool has_local_printing,
      const local_discovery::DeviceDescription& description) OVERRIDE;
  virtual void LocalPrinterRemoved(const std::string& name) OVERRIDE;
  virtual void LocalPrinterCacheFlushed() OVERRIDE;

  // PrivetLocalPrintOperation::Delegate implementation.
  virtual void OnPrivetPrintingDone(
      const local_discovery::PrivetLocalPrintOperation*
      print_operation) OVERRIDE;
  virtual void OnPrivetPrintingError(
      const local_discovery::PrivetLocalPrintOperation* print_operation,
        int http_code) OVERRIDE;
#endif  // ENABLE_SERVICE_DISCOVERY
  int regenerate_preview_request_count() const {
    return regenerate_preview_request_count_;
  }

  // Sets |pdf_file_saved_closure_| to |closure|.
  void SetPdfSavedClosureForTesting(const base::Closure& closure);

 private:
  friend class PrintPreviewPdfGeneratedBrowserTest;
  FRIEND_TEST_ALL_PREFIXES(PrintPreviewPdfGeneratedBrowserTest,
                           MANUAL_DummyTest);
  class AccessTokenService;

  static bool PrivetPrintingEnabled();

  content::WebContents* preview_web_contents() const;

  // Gets the list of printers. |args| is unused.
  void HandleGetPrinters(const base::ListValue* args);

  // Starts getting all local privet printers. |arg| is unused.
  void HandleGetPrivetPrinters(const base::ListValue* args);

  // Stops getting all local privet printers. |arg| is unused.
  void HandleStopGetPrivetPrinters(const base::ListValue* args);

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

  // Asks the initiator renderer to show the native print system dialog. |args|
  // is unused.
  void HandleShowSystemDialog(const base::ListValue* args);

  // Callback for the signin dialog to call once signin is complete.
  void OnSigninComplete();

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

  // Asks the browser to show the cloud print dialog. |args| is signle int with
  // page count.
  void HandlePrintWithCloudPrintDialog(const base::ListValue* args);

  // Asks the browser for several settings that are needed before the first
  // preview is displayed.
  void HandleGetInitialSettings(const base::ListValue* args);

  // Reports histogram data for a print preview UI action. |args| should consist
  // of two elements: the bucket name, and the bucket event.
  void HandleReportUiEvent(const base::ListValue* args);

  // Forces the opening of a new tab. |args| should consist of one element: the
  // URL to set the new tab to.
  //
  // NOTE: This is needed to open FedEx confirmation window as a new tab.
  // Javascript's "window.open" opens a new window popup (since initiated from
  // async HTTP request) and worse yet, on Windows and Chrome OS, the opened
  // window opens behind the initiator window.
  void HandleForceOpenNewTab(const base::ListValue* args);

  void HandleGetPrivetPrinterCapabilities(const base::ListValue* arg);

  void SendInitialSettings(const std::string& default_printer);

  // Send OAuth2 access token.
  void SendAccessToken(const std::string& type,
                       const std::string& access_token);

  // Sends the printer capabilities to the Web UI. |settings_info| contains
  // printer capabilities information.
  void SendPrinterCapabilities(const base::DictionaryValue* settings_info);

  // Sends error notification to the Web UI when unable to return the printer
  // capabilities.
  void SendFailedToGetPrinterCapabilities(const std::string& printer_name);

  // Send the list of printers to the Web UI.
  void SetupPrinterList(const base::ListValue* printers);

  // Send whether cloud print integration should be enabled.
  void SendCloudPrintEnabled();

  // Send the PDF data to the cloud to print.
  void SendCloudPrintJob(const base::RefCountedBytes* data);

  // Handles printing to PDF.
  void PrintToPdf();

  // Asks the browser to show the cloud print dialog.
  void PrintWithCloudPrintDialog();

  // Gets the initiator for the print preview dialog.
  content::WebContents* GetInitiator() const;

  // Closes the preview dialog.
  void ClosePreviewDialog();

  // Adds all the recorded stats taken so far to histogram counts.
  void ReportStats();

  // Clears initiator details for the print preview dialog.
  void ClearInitiatorDetails();

  // Posts a task to save |data| to pdf at |print_to_pdf_path_|.
  void PostPrintToPdfTask();

  // Populates |settings| according to the current locale.
  void GetNumberFormatAndMeasurementSystem(base::DictionaryValue* settings);

  bool GetPreviewDataAndTitle(scoped_refptr<base::RefCountedBytes>* data,
                              base::string16* title) const;

#if defined(USE_CUPS)
  void SaveCUPSColorSetting(const base::DictionaryValue* settings);

  void ConvertColorSettingToCUPSColorModel(
      base::DictionaryValue* settings) const;
#endif

#if defined(ENABLE_SERVICE_DISCOVERY)
  void OnPrivetCapabilities(const base::DictionaryValue* capabilities);
  void PrivetCapabilitiesUpdateClient(
      scoped_ptr<local_discovery::PrivetHTTPClient> http_client);
  void PrivetLocalPrintUpdateClient(
      std::string print_ticket,
      std::string capabilities,
      gfx::Size page_size,
      scoped_ptr<local_discovery::PrivetHTTPClient> http_client);
  bool PrivetUpdateClient(
      scoped_ptr<local_discovery::PrivetHTTPClient> http_client);
  void StartPrivetLocalPrint(const std::string& print_ticket,
                             const std::string& capabilities,
                             const gfx::Size& page_size);
  void SendPrivetCapabilitiesError(const std::string& id);
  void PrintToPrivetPrinter(const std::string& printer_name,
                            const std::string& print_ticket,
                            const std::string& capabilities,
                            const gfx::Size& page_size);
  bool CreatePrivetHTTP(
      const std::string& name,
      const local_discovery::PrivetHTTPAsynchronousFactory::ResultCallback&
          callback);
  void FillPrinterDescription(
      const std::string& name,
      const local_discovery::DeviceDescription& description,
      bool has_local_printing,
      base::DictionaryValue* printer_value);
#endif

  // Register/unregister from notifications of changes done to the GAIA
  // cookie.
  void RegisterForMergeSession();
  void UnregisterForMergeSession();

  // The underlying dialog object.
  scoped_refptr<ui::SelectFileDialog> select_file_dialog_;

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
  scoped_ptr<AccessTokenService> token_service_;

  // Pointer to reconcilor so that print preview can listen for GAIA cookie
  // changes.
  AccountReconcilor* reconcilor_;

#if defined(ENABLE_SERVICE_DISCOVERY)
  scoped_refptr<local_discovery::ServiceDiscoverySharedClient>
      service_discovery_client_;
  scoped_ptr<local_discovery::PrivetLocalPrinterLister> printer_lister_;

  scoped_ptr<local_discovery::PrivetHTTPAsynchronousFactory>
      privet_http_factory_;
  scoped_ptr<local_discovery::PrivetHTTPResolution> privet_http_resolution_;
  scoped_ptr<local_discovery::PrivetV1HTTPClient> privet_http_client_;
  scoped_ptr<local_discovery::PrivetJSONOperation>
      privet_capabilities_operation_;
  scoped_ptr<local_discovery::PrivetLocalPrintOperation>
      privet_local_print_operation_;
#endif

  base::WeakPtrFactory<PrintPreviewHandler> weak_factory_;

  // Notifies tests that want to know if the PDF has been saved. This doesn't
  // notify the test if it was a successful save, only that it was attempted.
  base::Closure pdf_file_saved_closure_;

  DISALLOW_COPY_AND_ASSIGN(PrintPreviewHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_PRINT_PREVIEW_HANDLER_H_
