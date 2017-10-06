// Copyright 2017 the Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/printer_detector.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace {

// Fake PrinterDetector::Observer which is used to test the PrinterDetector
// class.
class FakePrinterDetectorObserver : public PrinterDetector::Observer {
 public:
  FakePrinterDetectorObserver() : on_printers_found_calls_(0) {}

  void OnPrintersFound(
      const std::vector<PrinterDetector::DetectedPrinter>& printers) override;

  void OnPrinterScanComplete() override {}

  int OnPrintersFoundCalls() const { return on_printers_found_calls_; }

 private:
  int on_printers_found_calls_;
};

void FakePrinterDetectorObserver::OnPrintersFound(
    const std::vector<PrinterDetector::DetectedPrinter>& printers) {
  ++on_printers_found_calls_;
}

class PrinterDetectorTest : public testing::Test {
 public:
  PrinterDetectorTest() {}

  ~PrinterDetectorTest() override {}

  void TestStartObservers(PrinterDetector* printer_detector) {
    FakePrinterDetectorObserver fake_printer_detector_observer;
    printer_detector->AddObserver(&fake_printer_detector_observer);
    EXPECT_EQ(0, fake_printer_detector_observer.OnPrintersFoundCalls());

    printer_detector->StartObservers();
    content::RunAllPendingInMessageLoop();

    EXPECT_EQ(1, fake_printer_detector_observer.OnPrintersFoundCalls());

    printer_detector->RemoveObserver(&fake_printer_detector_observer);
  }

 protected:
  content::TestBrowserThreadBundle test_browser_thread_bundle_;
};

}  // namespace
}  // namespace chromeos
