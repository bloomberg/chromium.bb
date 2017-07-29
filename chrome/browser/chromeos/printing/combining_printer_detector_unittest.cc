// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/combining_printer_detector.h"

#include <algorithm>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/observer_list.h"
#include "base/strings/string_util.h"
#include "chromeos/printing/printer_configuration.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {
// Fake printer detectors used as the source of detections to be combined.
class FakePrinterDetector : public PrinterDetector {
 public:
  explicit FakePrinterDetector(
      const std::vector<std::string>& starting_printers)
      : printer_ids_(starting_printers.begin(), starting_printers.end()) {}

  ~FakePrinterDetector() override = default;

  void Start() override {}

  // Add these printers to the list of detected printers, and notify
  // downstream observers.
  void AddPrinters(const std::vector<std::string>& ids) {
    for (const std::string& id : ids) {
      CHECK(printer_ids_.insert(id).second)
          << "Cowardly refusing to add printer with duplicate id " << id;
    }
    Notify();
  }

  // Remove these printers from the list of detected printers, and notify
  // downstream observers.
  void RemovePrinters(const std::vector<std::string>& ids) {
    for (const std::string id : ids) {
      CHECK_EQ(printer_ids_.erase(id), static_cast<size_t>(1))
          << "Can't remove printer with nonexistant id " << id;
    }
    Notify();
  }

  // Tell downstream observers that we're done finding printers.
  void PrinterScanComplete() {
    for (PrinterDetector::Observer& observer : observer_list_) {
      observer.OnPrinterScanComplete();
    }
  }

  // PrinterDetector implementations.
  void AddObserver(PrinterDetector::Observer* observer) override {
    observer_list_.AddObserver(observer);
  }

  void RemoveObserver(PrinterDetector::Observer* observer) override {
    observer_list_.RemoveObserver(observer);
  }

  std::vector<DetectedPrinter> GetPrinters() override {
    std::vector<DetectedPrinter> printers;
    for (const std::string& id : printer_ids_) {
      DetectedPrinter detected;
      detected.printer.set_id(id);
      printers.push_back(detected);
    }
    return printers;
  }

 private:
  void Notify() {
    const std::vector<DetectedPrinter> printers = GetPrinters();
    for (PrinterDetector::Observer& observer : observer_list_) {
      observer.OnPrintersFound(printers);
    }
  }

  base::ObserverList<PrinterDetector::Observer> observer_list_;

  // In the fake, we only use printer ids to figure out if the right printers
  // are being propagated.  So internally we just track ids and construct
  // printers on the fly when asked for them.
  std::set<std::string> printer_ids_;
};

class PrintingCombiningPrinterDetectorTest : public testing::Test,
                                             public PrinterDetector::Observer {
 public:
  void Init(const std::vector<std::vector<std::string>>& id_lists) {
    combining_detector_ = CombiningPrinterDetector::Create();

    bool test_owned = false;
    for (const std::vector<std::string>& id_list : id_lists) {
      // Divide up fake detectors between owned and unowned to exercise both
      // paths.  It shouldn't really matter in the test below where ownership of
      // a specific fake detector lies.
      auto fake_detector = base::MakeUnique<FakePrinterDetector>(id_list);
      fake_detectors_.push_back(fake_detector.get());
      if (test_owned) {
        combining_detector_->AddDetector(fake_detector.get());
        owned_fake_detectors_.emplace_back(std::move(fake_detector));
      } else {
        combining_detector_->AddOwnedDetector(std::move(fake_detector));
      }
      test_owned = !test_owned;
    }
    combining_detector_->AddObserver(this);
    combining_detector_->Start();
    printers_ = combining_detector_->GetPrinters();
  }

  void OnPrintersFound(
      const std::vector<PrinterDetector::DetectedPrinter>& printers) override {
    printers_ = printers;
  }

  void OnPrinterScanComplete() override { scan_complete_ = true; }

 protected:
  // Testing utility function -- return true if the printers we got in the most
  // recent OnPrintersFound call are the same as printers (without considering
  // order).
  void ExpectFoundPrintersAre(const std::vector<std::string>& expected_ids) {
    std::vector<std::string> sorted_expected(expected_ids.begin(),
                                             expected_ids.end());
    std::vector<std::string> sorted_actual;
    for (const PrinterDetector::DetectedPrinter& printer : printers_) {
      sorted_actual.push_back(printer.printer.id());
    }
    std::sort(sorted_expected.begin(), sorted_expected.end());
    std::sort(sorted_actual.begin(), sorted_actual.end());
    if (sorted_expected != sorted_actual) {
      ADD_FAILURE() << "Printer ids mismatch.  Expected: {"
                    << base::JoinString(sorted_expected, ", ") << "}; Found: {"
                    << base::JoinString(sorted_actual, ", ") << "}";
    }
  }

  // Have we been notified that the scan is complete?
  bool scan_complete_ = false;

  // The printers in the most recent OnPrintersFound call.
  std::vector<PrinterDetector::DetectedPrinter> printers_;

  // The fake detectors plugged into the combining detector.
  std::vector<FakePrinterDetector*> fake_detectors_;

  // Cleanup pointers for fake detectors owned by the test.  (The
  // other fake detectors are owned by the combining_detector.
  std::vector<std::unique_ptr<FakePrinterDetector>> owned_fake_detectors_;

  std::unique_ptr<CombiningPrinterDetector> combining_detector_;
};

TEST_F(PrintingCombiningPrinterDetectorTest, BasicUsage) {
  Init({{"1a"}, std::vector<std::string>(), {"3a"}});
  ExpectFoundPrintersAre({"1a", "3a"});
  EXPECT_FALSE(scan_complete_);

  // Find some printers in the second detector.
  fake_detectors_[1]->AddPrinters({"2a", "2b", "2c"});
  ExpectFoundPrintersAre({"1a", "2a", "2b", "2c", "3a"});
  EXPECT_FALSE(scan_complete_);

  // Find some printers on the first detector.
  fake_detectors_[0]->AddPrinters({"1b"});
  ExpectFoundPrintersAre({"1a", "1b", "2a", "2b", "2c", "3a"});
  EXPECT_FALSE(scan_complete_);

  // Have the first detector signal completion, the combined detector should
  // not be complete until all 3 fake detectors have completed.
  fake_detectors_[0]->PrinterScanComplete();
  EXPECT_FALSE(scan_complete_);
  ExpectFoundPrintersAre({"1a", "1b", "2a", "2b", "2c", "3a"});

  // Exercise removal of a printer.
  fake_detectors_[2]->RemovePrinters({"3a"});
  EXPECT_FALSE(scan_complete_);
  ExpectFoundPrintersAre({"1a", "1b", "2a", "2b", "2c"});

  // Mark the 3rd detector as complete.  The 2nd detector has still not declared
  // itself complete.
  fake_detectors_[2]->PrinterScanComplete();
  EXPECT_FALSE(scan_complete_);
  ExpectFoundPrintersAre({"1a", "1b", "2a", "2b", "2c"});

  // Multiple scan complete calls from the same detector shouldn't happen, but
  // if they do happen, it should be a nop.
  fake_detectors_[2]->PrinterScanComplete();
  EXPECT_FALSE(scan_complete_);
  ExpectFoundPrintersAre({"1a", "1b", "2a", "2b", "2c"});

  // We can still add printers from a detector that has declared itself
  // complete.
  fake_detectors_[0]->AddPrinters({"1c"});
  ExpectFoundPrintersAre({"1a", "1b", "1c", "2a", "2b", "2c"});
  EXPECT_FALSE(scan_complete_);

  // Finally the 2nd detector declares itself complete, which should mean the
  // combined detector is now complete.
  fake_detectors_[1]->PrinterScanComplete();
  EXPECT_TRUE(scan_complete_);
  ExpectFoundPrintersAre({"1a", "1b", "1c", "2a", "2b", "2c"});

  // Should still be able to add printers even after completion.
  fake_detectors_[2]->AddPrinters({"3g", "3h", "3i"});
  EXPECT_TRUE(scan_complete_);
  ExpectFoundPrintersAre(
      {"1a", "1b", "1c", "2a", "2b", "2c", "3g", "3h", "3i"});
}

}  // namespace
}  // namespace chromeos
