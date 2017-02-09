// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PRINTING_PRINTER_DISCOVERER_H_
#define CHROME_BROWSER_CHROMEOS_PRINTING_PRINTER_DISCOVERER_H_

#include <memory>
#include <vector>

#include "chromeos/printing/printer_configuration.h"

namespace chromeos {

// Interface for printer discovery.  Constructs Printer objects from USB and
// zeroconf (DNS-SD) printers.
class CHROMEOS_EXPORT PrinterDiscoverer {
 public:
  // Interface for objects interested in detected printers.
  class Observer {
   public:
    // Called after discovery has started.
    virtual void OnDiscoveryStarted() {}

    // Called when discovery is stopping.  OnPrintersFound will not be
    // called after this occurs.
    virtual void OnDiscoveryStopping() {}

    // Called after all printers have been discovered and the observers
    // notified.
    virtual void OnDiscoveryDone() {}

    // Called with a collection of printers as they are discovered.  Printer
    // objects must be copied if they are retained outside of the scope of this
    // function.
    virtual void OnPrintersFound(const std::vector<Printer>& printers) {}
  };

  // Static factory
  static std::unique_ptr<PrinterDiscoverer> Create();

  virtual ~PrinterDiscoverer() {}

  // Begin scanning for printers.  Found printers will be reported to the
  // attached observer.
  virtual bool StartDiscovery() = 0;

  // Stop scanning for printers.  Returns false if scanning could not be stopped
  // and new results will continue to be be sent to observers.  Returns true if
  // scanning was stopped successfully.  No calls to the attached observer will
  // be made after this returns if it returned true.
  virtual bool StopDiscovery() = 0;

  // Add an observer that will be notified of discovered printers.  Ownership of
  // |observer| is not taken by the discoverer.  It is an error to add an
  // oberserver more than once.
  virtual void AddObserver(PrinterDiscoverer::Observer* observer) = 0;

  // Remove an observer of printer discovery.
  virtual void RemoveObserver(PrinterDiscoverer::Observer* observer) = 0;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PRINTING_PRINTER_DISCOVERER_H_
