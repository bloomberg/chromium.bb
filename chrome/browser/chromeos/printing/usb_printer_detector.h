// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PRINTING_USB_PRINTER_DETECTOR_H_
#define CHROME_BROWSER_CHROMEOS_PRINTING_USB_PRINTER_DETECTOR_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "chrome/browser/chromeos/printing/printer_detector.h"
#include "chromeos/printing/printer_configuration.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace chromeos {

// Observes device::UsbService for addition of USB printers.  When a new USB
// printer that is not already configured for this user is found, if it can be
// automatically configured for printing, that is done.  USB printers that
// cannot be automatically configured are exposed via the PrinterDetector
// interface so that higher level processing can handle them.
class UsbPrinterDetector : public PrinterDetector, public KeyedService {
 public:
  // Factory function for the CUPS implementation.
  static std::unique_ptr<UsbPrinterDetector> Create(Profile* profile);
  ~UsbPrinterDetector() override = default;

 protected:
  UsbPrinterDetector() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(UsbPrinterDetector);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PRINTING_USB_PRINTER_DETECTOR_H_
