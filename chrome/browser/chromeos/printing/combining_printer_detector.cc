// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/combining_printer_detector.h"

#include <map>
#include <utility>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/observer_list.h"
#include "base/scoped_observer.h"
#include "chrome/browser/chromeos/printing/printer_detector.h"
#include "chromeos/printing/printer_configuration.h"

namespace chromeos {
namespace {

class CombiningPrinterDetectorImpl;

// We can't just implement PrinterDetector::Observer in
// CombiningPrinterDetectorImpl because we need to be able to determing *which*
// underlying detector is giving us new information.  Thus we use delegate
// instantions to allow for a disambiguation about the source of a
// PrinterDetector callback from one of the detectors being combined.
class ObserverDelegate : public PrinterDetector::Observer {
 public:
  ObserverDelegate(CombiningPrinterDetectorImpl* parent,
                   PrinterDetector* detector)
      : parent_(parent), observer_(this) {
    observer_.Add(detector);
  }
  void OnPrintersFound(const std::vector<Printer>& printers) override;
  void OnPrinterScanComplete() override;

 private:
  CombiningPrinterDetectorImpl* parent_;
  ScopedObserver<PrinterDetector, PrinterDetector::Observer> observer_;
};

class CombiningPrinterDetectorImpl : public CombiningPrinterDetector {
 public:
  ~CombiningPrinterDetectorImpl() override = default;

  void AddDetector(PrinterDetector* detector) override {
    detectors_.push_back(detector);
    delegates_.push_back(base::MakeUnique<ObserverDelegate>(this, detector));
    printers_.insert({delegates_.back().get(), detector->GetPrinters()});
    scan_done_[delegates_.back().get()] = false;
  }

  void AddOwnedDetector(std::unique_ptr<PrinterDetector> detector) override {
    AddDetector(detector.get());
    owned_detectors_.emplace_back(std::move(detector));
  }

  // PrinterDetector implementations.
  void AddObserver(PrinterDetector::Observer* observer) override {
    observer_list_.AddObserver(observer);
  }

  void RemoveObserver(PrinterDetector::Observer* observer) override {
    observer_list_.RemoveObserver(observer);
  }

  // Note this is *not* the PrinterDetectorObserver interface directly,
  // it's the entry point for ObserverDelegates to pass along those messages
  // into this class.
  void OnPrintersFound(const ObserverDelegate* source,
                       const std::vector<Printer>& printers) {
    // If this object doesn't know about the delegate trying to update its
    // printers, we missed delegate registration somehow, which shouldn't
    // happen.
    DCHECK(base::ContainsKey(printers_, source));
    printers_[source] = printers;
    std::vector<Printer> all_printers = GetPrinters();
    for (PrinterDetector::Observer& observer : observer_list_) {
      observer.OnPrintersFound(all_printers);
    }
  }

  void OnPrinterScanComplete(const ObserverDelegate* source) {
    DCHECK(base::ContainsKey(printers_, source));
    bool& scan_done = scan_done_[source];
    if (!scan_done) {
      scan_done = true;
      for (const auto& entry : scan_done_) {
        if (!entry.second) {
          // Not all done yet.
          return;
        }
      }
      // Final outstanding scan just finished, notify observers.
      for (PrinterDetector::Observer& observer : observer_list_) {
        observer.OnPrinterScanComplete();
      }
    }
  }

  // Aggregate the printers from all underlying sources and return them.
  std::vector<Printer> GetPrinters() override {
    std::vector<Printer> ret;
    for (const auto& entry : printers_) {
      ret.insert(ret.end(), entry.second.begin(), entry.second.end());
    }
    return ret;
  }

  void Start() override {
    for (PrinterDetector* detector : detectors_) {
      detector->Start();
    }
  }

 private:
  // Map from observer delegate to the most recent list of printers from that
  // observer.
  std::map<const ObserverDelegate*, std::vector<Printer>> printers_;

  // Map from observer delegate to whether or not that observer has completed
  // its scan.
  std::map<const ObserverDelegate*, bool> scan_done_;

  // Observers of this object.
  base::ObserverList<PrinterDetector::Observer> observer_list_;

  std::vector<std::unique_ptr<PrinterDetector>> owned_detectors_;

  // Memory management -- this just exists to ensure that the held
  // elements get cleaned up.
  std::vector<std::unique_ptr<ObserverDelegate>> delegates_;

  // All detectors used by the combining detector, owned or unowned.
  std::vector<PrinterDetector*> detectors_;
};

void ObserverDelegate::OnPrintersFound(const std::vector<Printer>& printers) {
  parent_->OnPrintersFound(this, printers);
}

void ObserverDelegate::OnPrinterScanComplete() {
  parent_->OnPrinterScanComplete(this);
}

}  // namespace

// static
std::unique_ptr<CombiningPrinterDetector> CombiningPrinterDetector::Create() {
  return base::MakeUnique<CombiningPrinterDetectorImpl>();
}

}  // namespace chromeos
