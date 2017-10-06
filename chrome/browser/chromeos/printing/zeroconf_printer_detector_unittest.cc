// Copyright 2017 the Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/printer_detector.h"
#include "chrome/browser/chromeos/printing/printer_detector_test_util.h"
#include "chrome/browser/chromeos/printing/zeroconf_printer_detector.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace {

class ZeroconfPrinterDetectorTest : public PrinterDetectorTest {};

TEST_F(ZeroconfPrinterDetectorTest, ZeroconfPrinterDetectorStartObservers) {
  TestingProfile testing_profile;
  std::unique_ptr<PrinterDetector> zeroconf_printer_detector =
      ZeroconfPrinterDetector::Create(&testing_profile);

  TestStartObservers(zeroconf_printer_detector.get());
}

}  // namespace
}  // namespace chromeos
