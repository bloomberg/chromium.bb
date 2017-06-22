// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_CUPS_PRINTERS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_CUPS_PRINTERS_HANDLER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/printing/printer_configurer.h"
#include "chrome/browser/chromeos/printing/printer_detector.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "chromeos/printing/ppd_provider.h"
#include "chromeos/printing/printer_configuration.h"
#include "ui/shell_dialogs/select_file_dialog.h"

namespace base {
class ListValue;
}  // namespace base

class Profile;

namespace chromeos {
namespace printing {
class PpdProvider;
}

class CombiningPrinterDetector;

namespace settings {

// Chrome OS CUPS printing settings page UI handler.
class CupsPrintersHandler : public ::settings::SettingsPageUIHandler,
                            public ui::SelectFileDialog::Listener,
                            public PrinterDetector::Observer {
 public:
  explicit CupsPrintersHandler(content::WebUI* webui);
  ~CupsPrintersHandler() override;

  // SettingsPageUIHandler overrides:
  void RegisterMessages() override;
  void OnJavascriptAllowed() override {}
  void OnJavascriptDisallowed() override {}

 private:
  // Gets all CUPS printers and return it to WebUI.
  void HandleGetCupsPrintersList(const base::ListValue* args);
  void HandleUpdateCupsPrinter(const base::ListValue* args);
  void HandleRemoveCupsPrinter(const base::ListValue* args);

  // For a CupsPrinterInfo in |args|, retrieves the relevant PrinterInfo object
  // using an IPP call to the printer.
  void HandleGetPrinterInfo(const base::ListValue* args);

  // Handles the callback for HandleGetPrinterInfo. |callback_id| is the
  // identifier to resolve the correct Promise. |success| indicates if the
  // query was successful. |make| is the detected printer manufacturer.
  // |model| is the detected model. |ipp_everywhere| indicates if configuration
  // using the CUPS IPP Everywhere driver should be attempted. If |success| is
  // false, the values of |make|, |model| and |ipp_everywhere| are not
  // specified.
  void OnPrinterInfo(const std::string& callback_id,
                     bool success,
                     const std::string& make,
                     const std::string& model,
                     bool ipp_everywhere);

  void HandleAddCupsPrinter(const base::ListValue* args);
  void OnAddedPrinter(std::unique_ptr<Printer> printer,
                      PrinterSetupResult result);
  void OnAddPrinterError();

  // Get a list of all manufacturers for which we have at least one model of
  // printer supported.  Takes one argument, the callback id for the result.
  // The callback will be invoked with {success: <boolean>, models:
  // <Array<string>>}.
  void HandleGetCupsPrinterManufacturers(const base::ListValue* args);

  // Given a manufacturer, get a list of all models of printers for which we can
  // get drivers.  Takes two arguments - the callback id and the manufacturer
  // name for which we want to list models.  The callback will be called with
  // {success: <boolean>, models: Array<string>}.
  void HandleGetCupsPrinterModels(const base::ListValue* args);

  void HandleSelectPPDFile(const base::ListValue* args);

  // PpdProvider callback handlers.
  void ResolveManufacturersDone(
      const std::string& js_callback,
      printing::PpdProvider::CallbackResultCode result_code,
      const std::vector<std::string>& available);
  void ResolvePrintersDone(
      const std::string& js_callback,
      printing::PpdProvider::CallbackResultCode result_code,
      const std::vector<std::string>& available);

  // ui::SelectFileDialog::Listener override:
  void FileSelected(const base::FilePath& path,
                    int index,
                    void* params) override;

  void HandleStartDiscovery(const base::ListValue* args);
  void HandleStopDiscovery(const base::ListValue* args);

  // PrinterDetector::Observer implementations:
  void OnPrintersFound(const std::vector<Printer>& printers) override;
  void OnPrinterScanComplete() override;

  // Invokes debugd to add the printer to CUPS.  If |ipp_everywhere| is true,
  // automatic configuration will be attempted  and |ppd_path| is ignored.
  // |ppd_path| is the path to a Postscript Printer Description file that will
  // be used to configure the printer capabilities.  This file must be in
  // Downloads or the PPD Cache.
  void AddPrinterToCups(std::unique_ptr<Printer> printer,
                        const base::FilePath& ppd_path,
                        bool ipp_everywhere);

  std::unique_ptr<CombiningPrinterDetector> printer_detector_;
  scoped_refptr<printing::PpdProvider> ppd_provider_;
  std::unique_ptr<PrinterConfigurer> printer_configurer_;

  Profile* profile_;
  scoped_refptr<ui::SelectFileDialog> select_file_dialog_;
  std::string webui_callback_id_;

  base::WeakPtrFactory<CupsPrintersHandler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(CupsPrintersHandler);
};

}  // namespace settings
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_CUPS_PRINTERS_HANDLER_H_
