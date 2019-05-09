// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PRINTING_AUTOMATIC_USB_PRINTER_CONFIGURER_H_
#define CHROME_BROWSER_CHROMEOS_PRINTING_AUTOMATIC_USB_PRINTER_CONFIGURER_H_

#include <memory>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "chrome/browser/chromeos/printing/cups_printers_manager.h"
#include "chrome/browser/chromeos/printing/printer_configurer.h"
#include "chrome/browser/chromeos/printing/printer_installation_manager.h"
#include "chromeos/printing/printer_configuration.h"

namespace chromeos {

class AutomaticUsbPrinterConfigurer : public CupsPrintersManager::Observer {
 public:
  AutomaticUsbPrinterConfigurer(
      std::unique_ptr<PrinterConfigurer> printer_configurer_,
      PrinterInstallationManager* installation_manager);

  ~AutomaticUsbPrinterConfigurer() override;

  // CupsPrintersManager::Observer override.
  void OnPrintersChanged(PrinterClass printer_class,
                         const std::vector<Printer>& printers) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(AutomaticUsbPrinterConfigurerTest,
                           UsbPrinterAddedToSet);
  FRIEND_TEST_ALL_PREFIXES(AutomaticUsbPrinterConfigurerTest,
                           UsbPrinterRemovedFromSet);

  // Uses |printer_configurer_| to setup |printer| if it is not yet setup.
  void SetupPrinter(const Printer& printer);

  // Callback for PrinterConfiguer::SetUpPrinter().
  void OnSetupComplete(const Printer& printer, PrinterSetupResult result);

  // Completes the configuration for |printer|. Saves printer in |printers_|.
  void CompleteConfiguration(const Printer& printer);

  // Removes any printers from |printers_| that are no longer in
  // |automatic_printers|.
  void PruneRemovedPrinters(const std::vector<Printer>& automatic_printers);

  SEQUENCE_CHECKER(sequence_);

  std::unique_ptr<PrinterConfigurer> printer_configurer_;
  PrinterInstallationManager* installation_manager_;  // Not owned.
  base::flat_set<std::string> printers_;

  base::WeakPtrFactory<AutomaticUsbPrinterConfigurer> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AutomaticUsbPrinterConfigurer);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PRINTING_AUTOMATIC_USB_PRINTER_CONFIGURER_H_
