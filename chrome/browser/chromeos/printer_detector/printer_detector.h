// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PRINTER_DETECTOR_PRINTER_DETECTOR_H_
#define CHROME_BROWSER_CHROMEOS_PRINTER_DETECTOR_PRINTER_DETECTOR_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "device/usb/usb_service.h"

class NotificationUIManager;
class Profile;

namespace device {
class UsbDevice;
}

namespace chromeos {

// Observes device::UsbService for addition of USB printers (devices with
// interface class 7).
// When a printer is detected, a shows a notification depending on whether there
// are printer provider apps that declared support for the USB device installed.
// If such app exists, the notification notifies the user the printer is ready.
// Otherwise the notification offers user to search Chrome Web Store for apps
// that support the printer. Clicking the notification launches webstore_widget
// app for the printer.
// The notification is shown only for active user's profile.
class PrinterDetector : public KeyedService,
                        public device::UsbService::Observer {
 public:
  explicit PrinterDetector(Profile* profile);
  ~PrinterDetector() override;

 private:
  friend class PrinterDetectorAppSearchEnabledTest;

  void SetNotificationUIManagerForTesting(NotificationUIManager* manager);

  // KeyedService override:
  void Shutdown() override;

  // UsbService::observer override:
  void OnDeviceAdded(scoped_refptr<device::UsbDevice> device) override;

  // Initializes the printer detector.
  void Initialize();

  Profile* profile_;
  NotificationUIManager* notification_ui_manager_;
  ScopedObserver<device::UsbService, device::UsbService::Observer> observer_;
  base::WeakPtrFactory<PrinterDetector> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PrinterDetector);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PRINTER_DETECTOR_PRINTER_DETECTOR_H_
