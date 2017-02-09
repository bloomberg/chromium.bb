// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PRINTING_FAKE_PRINTER_DISCOVERER_H_
#define CHROME_BROWSER_CHROMEOS_PRINTING_FAKE_PRINTER_DISCOVERER_H_

#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/printing/printer_discoverer.h"
#include "chromeos/printing/printer_configuration.h"

namespace chromeos {

class FakePrinterDiscoverer : public PrinterDiscoverer {
 public:
  FakePrinterDiscoverer();
  ~FakePrinterDiscoverer() override;

  bool StartDiscovery() override;
  bool StopDiscovery() override;
  void AddObserver(PrinterDiscoverer::Observer* observer) override;
  void RemoveObserver(PrinterDiscoverer::Observer* observer) override;

 private:
  void EmitPrinters(size_t start, size_t end);

  std::vector<chromeos::Printer> printers_;
  bool discovery_running_;
  std::vector<PrinterDiscoverer::Observer*> observers_;

  base::WeakPtrFactory<FakePrinterDiscoverer> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(FakePrinterDiscoverer);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PRINTING_FAKE_PRINTER_DISCOVERER_H_
