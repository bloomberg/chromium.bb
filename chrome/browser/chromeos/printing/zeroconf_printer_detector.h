// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_CHROMEOS_PRINTING_ZEROCONF_PRINTER_DETECTOR_H_
#define CHROME_BROWSER_CHROMEOS_PRINTING_ZEROCONF_PRINTER_DETECTOR_H_

#include <memory>

#include "chrome/browser/chromeos/printing/printer_detector.h"

class Profile;

namespace chromeos {

// Use mDNS and DNS-SD to detect nearby networked printers.  This
// is sometimes called zeroconf, or Bonjour.
class ZeroconfPrinterDetector : public PrinterDetector {
 public:
  ~ZeroconfPrinterDetector() override = default;

  static std::unique_ptr<ZeroconfPrinterDetector> Create(Profile* profile);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PRINTING_ZEROCONF_PRINTER_DETECTOR_H_
