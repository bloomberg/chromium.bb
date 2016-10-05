// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_CUPS_PRINTERS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_CUPS_PRINTERS_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "chromeos/printing/printer_configuration.h"
#include "chromeos/printing/printer_discoverer.h"
#include "ui/shell_dialogs/select_file_dialog.h"

namespace base {
class ListValue;
}  // namespace base

class Profile;

namespace chromeos {
namespace settings {

// Chrome OS CUPS printing settings page UI handler.
class CupsPrintersHandler : public ::settings::SettingsPageUIHandler,
                            public ui::SelectFileDialog::Listener,
                            public chromeos::PrinterDiscoverer::Observer {
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

  void HandleAddCupsPrinter(const base::ListValue* args);
  void OnAddedPrinter(std::unique_ptr<Printer> printer, bool success);
  void OnAddPrinterError();

  void HandleSelectPPDFile(const base::ListValue* args);

  // ui::SelectFileDialog::Listener override:
  void FileSelected(const base::FilePath& path,
                    int index,
                    void* params) override;

  void HandleStartDiscovery(const base::ListValue* args);
  void HandleStopDiscovery(const base::ListValue* args);

  // chromeos::PrinterDiscoverer::Observer override:
  void OnPrintersFound(const std::vector<Printer>& printers) override;
  void OnDiscoveryDone() override;

  std::unique_ptr<chromeos::PrinterDiscoverer> printer_discoverer_;

  Profile* profile_;
  scoped_refptr<ui::SelectFileDialog> select_file_dialog_;
  std::string webui_callback_id_;

  base::WeakPtrFactory<CupsPrintersHandler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(CupsPrintersHandler);
};

}  // namespace settings
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_CUPS_PRINTERS_HANDLER_H_
