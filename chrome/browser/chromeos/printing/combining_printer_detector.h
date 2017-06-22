// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PRINTING_COMBINING_PRINTER_DETECTOR_H_
#define CHROME_BROWSER_CHROMEOS_PRINTING_COMBINING_PRINTER_DETECTOR_H_

#include <memory>

#include "chrome/browser/chromeos/printing/printer_detector.h"

namespace chromeos {

// CombiningPrinterDetector implements PrinterDetector and exports the combined
// results of one or more underlying PrinterDetectors.
class CHROMEOS_EXPORT CombiningPrinterDetector : public PrinterDetector {
 public:
  // Static factory
  static std::unique_ptr<CombiningPrinterDetector> Create();

  ~CombiningPrinterDetector() override = default;

  // Add an underlying detector to be used.  Ownership is not taken.  All
  // detectors should be added on the same sequence.  Calling AddDetector after
  // observers have been added is disallowed.
  virtual void AddDetector(PrinterDetector* detector) = 0;

  // Same as AddDetector, but ownership of the detector is taken.
  virtual void AddOwnedDetector(std::unique_ptr<PrinterDetector> detector) = 0;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PRINTING_COMBINING_PRINTER_DETECTOR_H_
