// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PRINTER_DETECTOR_USB_PRINTER_DETECTOR_H_
#define CHROME_BROWSER_CHROMEOS_PRINTER_DETECTOR_USB_PRINTER_DETECTOR_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "chromeos/printing/printer_configuration.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace chromeos {

// Observes device::UsbService for addition of USB printers (devices with
// interface class 7).  When a device is detected, it is forwarded to the
// printing subsystem for either autoconfiguration or user guidance.
class UsbPrinterDetector : public KeyedService {
 public:
  class Observer {
   public:
    virtual ~Observer() = default;

    // The set of available printers has changed.
    virtual void OnAvailableUsbPrintersChanged(
        const std::vector<Printer>& printers) = 0;
  };

  // Factory function for the CUPS implementation.
  static std::unique_ptr<UsbPrinterDetector> Create(Profile* profile);
  ~UsbPrinterDetector() override = default;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Get the current set of detected printers.
  virtual std::vector<Printer> GetPrinters() = 0;

 protected:
  UsbPrinterDetector() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(UsbPrinterDetector);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PRINTER_DETECTOR_USB_PRINTER_DETECTOR_H_
