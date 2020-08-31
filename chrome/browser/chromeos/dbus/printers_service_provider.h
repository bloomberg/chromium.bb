// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DBUS_PRINTERS_SERVICE_PROVIDER_H_
#define CHROME_BROWSER_CHROMEOS_DBUS_PRINTERS_SERVICE_PROVIDER_H_

#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/scoped_observer.h"
#include "chrome/browser/chromeos/printing/cups_printers_manager.h"
#include "chrome/browser/chromeos/printing/cups_printers_manager_proxy.h"
#include "chromeos/dbus/services/cros_dbus_service.h"
#include "dbus/exported_object.h"

namespace chromeos {

// Provides a DBus service which emits a signal to indicate that the available
// printers has changed.  Clients are intended to listen for the signal then
// make a request for more printers through a side channel e.g. cups_proxy.
class PrintersServiceProvider
    : public CrosDBusService::ServiceProviderInterface,
      public chromeos::CupsPrintersManager::Observer {
 public:
  PrintersServiceProvider();
  ~PrintersServiceProvider() override;

  // CrosDBusService::ServiceProviderInterface overrides:
  void Start(scoped_refptr<dbus::ExportedObject> exported_object) override;

  // CupsPrintersManager::Observer overrides:
  void OnPrintersChanged(PrinterClass printers_class,
                         const std::vector<Printer>& printers) override;

 private:
  // Emits the D-Bus signal for this event.
  void EmitSignal();

  // A reference on ExportedObject for sending signals.
  scoped_refptr<dbus::ExportedObject> exported_object_;

  ScopedObserver<chromeos::CupsPrintersManagerProxy,
                 chromeos::CupsPrintersManager::Observer>
      printers_manager_observer_{this};

  DISALLOW_COPY_AND_ASSIGN(PrintersServiceProvider);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_DBUS_PRINTERS_SERVICE_PROVIDER_H_
