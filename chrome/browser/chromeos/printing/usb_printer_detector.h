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
#include "device/usb/usb_service.h"

namespace chromeos {

// Observes device::UsbService for addition of USB printers, and implements the
// PrinterDetector interface to export this to print system consumers.
class UsbPrinterDetector : public PrinterDetector {
 public:
  // Factory function for the CUPS implementation.
  static std::unique_ptr<UsbPrinterDetector> Create();
  static std::unique_ptr<UsbPrinterDetector> CreateForTesting(
      device::UsbService*);
  ~UsbPrinterDetector() override = default;

 protected:
  UsbPrinterDetector() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(UsbPrinterDetector);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PRINTING_USB_PRINTER_DETECTOR_H_
