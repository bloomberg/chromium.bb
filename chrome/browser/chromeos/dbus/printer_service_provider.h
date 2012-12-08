// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DBUS_PRINTER_SERVICE_PROVIDER_H_
#define CHROME_BROWSER_CHROMEOS_DBUS_PRINTER_SERVICE_PROVIDER_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/dbus/cros_dbus_service.h"
#include "dbus/exported_object.h"

namespace dbus {
class MethodCall;
class Response;
}

namespace chromeos {

// This class provides printer service for CrosDBusService.
// It detects attached printers and show user help page.
//
// The following methods are exported.
//
// Interface: org.chromium.LibCrosServiceInterface (kLibCrosServiceInterface)
// Method: PrinterAdded (kPrinterAdded)
// Parameters: string:vendor Optional, USB vendor ID.
//             string:product Optional, USB product ID.
//
//   Display help page to advice user to use Cloud Print.
//
//   The returned signal will contain the three values:
//
// This service can be manually tested dbus-send on ChromeOS.
//
// 1. Open a terminal and run the following:
//
//   % dbus-send --system --type=method_call
//       --dest=org.chromium.LibCrosService /org/chromium/LibCrosService
//       org.chromium.LibCrosServiceInterface.PrinterAdded
//       string:123 string:456
//
// 2. Go back to ChromeOS and check if new tab with information is opened.

class PrinterServiceProvider
    : public CrosDBusService::ServiceProviderInterface {
 public:
  PrinterServiceProvider();
  virtual ~PrinterServiceProvider();

  // CrosDBusService::ServiceProviderInterface override.
  virtual void Start(
      scoped_refptr<dbus::ExportedObject> exported_object) OVERRIDE;

 protected:
  virtual void ShowCloudPrintHelp(const std::string& vendor,
                                  const std::string& product);

 private:
  // Called from ExportedObject, when PrinterAdded() is exported as
  // a D-Bus method, or failed to be exported.
  void OnExported(const std::string& interface_name,
                  const std::string& method_name,
                  bool success);

  // Invoked when usb printer is detected.
  // Called on UI thread from dbus request.
  void PrinterAdded(dbus::MethodCall* method_call,
                    dbus::ExportedObject::ResponseSender response_sender);

  scoped_refptr<dbus::ExportedObject> exported_object_;
  base::WeakPtrFactory<PrinterServiceProvider> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PrinterServiceProvider);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_DBUS_PRINTER_SERVICE_PROVIDER_H_

