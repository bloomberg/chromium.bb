// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PRINTING_PRINTER_DISCOVERER_H_
#define CHROME_BROWSER_CHROMEOS_PRINTING_PRINTER_DISCOVERER_H_

#include <memory>
#include <vector>

#include "chromeos/printing/printer_configuration.h"

class Profile;

namespace chromeos {

// Interface for printer discovery.  Constructs Printer objects from USB and
// zeroconf (DNS-SD) printers.  All functions in this class must be called
// from a sequenced context.
class CHROMEOS_EXPORT PrinterDiscoverer {
 public:
  // Interface for objects interested in detected printers.
  class Observer {
   public:
    virtual ~Observer() = default;

    // Called when we are done with the initial scan for printers.  We may
    // still call OnPrintersFound if the set of available printers
    // changes, but the user can conclude that if a printer is currently
    // available and not in the list, we're not still looking for it.
    // TODO(justincarlson): Merge with OnPrintersFound when crbug.com/588234 is
    // complete.
    virtual void OnDiscoveryInitialScanDone(int printer_count) = 0;

    // Called with a collection of printers as they are discovered.  On each
    // call |printers| is the full set of known printers; it is not
    // incremental; printers may be added or removed.
    //
    // Observers will get an OnPrintersFound callback after registration
    // with the existing list of printers (which may be empty) and will get
    // additional calls whenever the set of printers changes.
    virtual void OnPrintersFound(const std::vector<Printer>& printers) = 0;
  };

  // Static factory
  static std::unique_ptr<PrinterDiscoverer> CreateForProfile(Profile* profile);

  virtual ~PrinterDiscoverer() = default;

  // Add an observer that will be notified of discovered printers.  Ownership of
  // |observer| is not taken by the discoverer.  It is an error to add an
  // observer more than once.  Calls to |observer| methods will take place on
  // the thread PrinterDiscoverer was instantiated on.
  virtual void AddObserver(PrinterDiscoverer::Observer* observer) = 0;

  // Remove an observer of printer discovery.
  virtual void RemoveObserver(PrinterDiscoverer::Observer* observer) = 0;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PRINTING_PRINTER_DISCOVERER_H_
