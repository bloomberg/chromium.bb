// Copyright 2017 the Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/printer_detector.h"
#include "chrome/browser/chromeos/printing/printer_detector_test_util.h"
#include "chrome/browser/chromeos/printing/usb_printer_detector.h"
#include "device/usb/mock_usb_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace {

class UsbPrinterDetectorTest : public PrinterDetectorTest {};

TEST_F(UsbPrinterDetectorTest, UsbPrinterDetectorStartObservers) {
  device::MockUsbService mock_usb_service;
  std::unique_ptr<PrinterDetector> usb_printer_detector =
      UsbPrinterDetector::CreateForTesting(&mock_usb_service);

  TestStartObservers(usb_printer_detector.get());
}

}  // namespace
}  // namespace chromeos
