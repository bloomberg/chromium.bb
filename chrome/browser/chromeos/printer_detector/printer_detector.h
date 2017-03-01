// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PRINTER_DETECTOR_PRINTER_DETECTOR_H_
#define CHROME_BROWSER_CHROMEOS_PRINTER_DETECTOR_PRINTER_DETECTOR_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "components/keyed_service/core/keyed_service.h"

class NotificationUIManager;
class Profile;

namespace chromeos {

// Observes device::UsbService for addition of USB printers (devices with
// interface class 7).  What it does with this depends on whether or not
// CUPS printing support is enabled.
//
// If CUPS is disabled, the Legacy implementation should be used.  The legacy
// implementation shows a notification depending on whether there are printer
// provider apps that declared support for the USB device installed.  If such
// app exists, the notification notifies the user the printer is ready.
// Otherwise the notification offers user to search Chrome Web Store for apps
// that support the printer. Clicking the notification launches webstore_widget
// app for the printer.  The notification is shown only for active user's
// profile.
//
// If CUPS is enabled, the Cups implementation should be used.  This
// implementation to guides the user through setting up a new USB printer in the
// CUPS backend.
class PrinterDetector : public KeyedService {
 public:
  // Factory function for the Legacy implementation.
  static std::unique_ptr<PrinterDetector> CreateLegacy(Profile* profile);

  // Factory function for the CUPS implementation.
  static std::unique_ptr<PrinterDetector> CreateCups(Profile* profile);
  ~PrinterDetector() override {}

 protected:
  PrinterDetector() = default;

 private:
  friend class PrinterDetectorAppSearchEnabledTest;

  virtual void SetNotificationUIManagerForTesting(
      NotificationUIManager* manager) = 0;

  DISALLOW_COPY_AND_ASSIGN(PrinterDetector);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PRINTER_DETECTOR_PRINTER_DETECTOR_H_
