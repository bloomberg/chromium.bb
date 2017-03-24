// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PRINTING_PRINTER_CONFIGURER_H_
#define CHROME_BROWSER_CHROMEOS_PRINTING_PRINTER_CONFIGURER_H_

#include <memory>

#include "base/callback_forward.h"
#include "chromeos/printing/printer_configuration.h"

class Profile;

namespace chromeos {

enum PrinterSetupResult {
  FATAL_ERROR,
  SUCCESS,              // Printer set up successfully
  PRINTER_UNREACHABLE,  // Could not reach printer
  DBUS_ERROR,           // Could not contact debugd

  // PPD errors
  PPD_TOO_LARGE,     // PPD exceeds size limit
  INVALID_PPD,       // PPD rejected by cupstestppd
  PPD_NOT_FOUND,     // Could not find PPD
  PPD_UNRETRIEVABLE  // Could not download PPD
};

using PrinterSetupCallback = base::Callback<void(PrinterSetupResult)>;

// Configures printers by retrieving PPDs and registering the printer with CUPS.
// Class must be constructed and used on the UI thread.
class PrinterConfigurer {
 public:
  static std::unique_ptr<PrinterConfigurer> Create(Profile* profile);

  PrinterConfigurer(const PrinterConfigurer&) = delete;
  PrinterConfigurer& operator=(const PrinterConfigurer&) = delete;

  virtual ~PrinterConfigurer() = default;

  // Set up |printer| retrieving the appropriate PPD and registering the printer
  // with CUPS.  |callback| is called with the result of the operation.  This
  // method must be called on the UI thread and will run |callback| on the
  // UI thread.
  virtual void SetUpPrinter(const Printer& printer,
                            const PrinterSetupCallback& callback) = 0;

 protected:
  PrinterConfigurer() = default;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PRINTING_PRINTER_CONFIGURER_H_
