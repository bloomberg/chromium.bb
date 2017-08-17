// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/cups_printers_manager.h"

#include <algorithm>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/memory/singleton.h"
#include "base/observer_list.h"
#include "base/scoped_observer.h"
#include "chrome/browser/chromeos/printing/ppd_provider_factory.h"
#include "chrome/browser/chromeos/printing/synced_printers_manager.h"
#include "chrome/browser/chromeos/printing/synced_printers_manager_factory.h"
#include "chrome/browser/chromeos/printing/usb_printer_detector.h"
#include "chrome/browser/chromeos/printing/usb_printer_detector_factory.h"
#include "chrome/browser/profiles/profile.h"

namespace chromeos {
namespace {

class CupsPrintersManagerImpl;

// Since CupsPrintersManager listens to multiple PrinterDetectors, we need to
// disambiguate incoming observer calls based on their source, and so can't
// implement PrinterDetector::Observer directly in CupsPrintersManagerImpl.
class PrinterDetectorObserverProxy : public PrinterDetector::Observer {
 public:
  PrinterDetectorObserverProxy(CupsPrintersManagerImpl* parent,
                               int id,
                               PrinterDetector* detector)
      : parent_(parent), id_(id), observer_(this) {
    observer_.Add(detector);
  }
  ~PrinterDetectorObserverProxy() override = default;

  // Defined out of line because we need the CupsPrintersManagerImpl
  // definition first.
  void OnPrintersFound(
      const std::vector<PrinterDetector::DetectedPrinter>& printers) override;

  // We don't do anything with OnPrinterScanComplete() at the moment, so just
  // stub it out.
  void OnPrinterScanComplete() override {}

 private:
  CupsPrintersManagerImpl* parent_;
  int id_;
  ScopedObserver<PrinterDetector, PrinterDetector::Observer> observer_;
};

// Return the set of ids for the given list of printers.
std::unordered_set<std::string> GetIdsSet(
    const std::vector<Printer>& printers) {
  std::unordered_set<std::string> ret;
  for (const Printer& printer : printers) {
    ret.insert(printer.id());
  }
  return ret;
}

// This is akin to python's filter() builtin, but with reverse polarity on the
// test function -- *remove* all entries in printers for which test_fn returns
// true, discard the rest.
void FilterOutPrinters(std::vector<Printer>* printers,
                       std::function<bool(const Printer&)> test_fn) {
  auto new_end = std::remove_if(printers->begin(), printers->end(), test_fn);
  printers->resize(new_end - printers->begin());
}

class CupsPrintersManagerImpl : public CupsPrintersManager,
                                public SyncedPrintersManager::Observer {
 public:
  // Identifiers for each of the underlying PrinterDetectors this
  // class observes.
  enum DetectorIds {
    kUsbDetector,
    kZeroconfDetector,
  };

  CupsPrintersManagerImpl(SyncedPrintersManager* synced_printers_manager,
                          PrinterDetector* usb_detector,
                          PrinterDetector* zeroconf_detector,
                          scoped_refptr<PpdProvider> ppd_provider)
      : synced_printers_manager_(synced_printers_manager),
        synced_printers_manager_observer_(this),
        usb_detector_(usb_detector),
        usb_detector_observer_proxy_(this, kUsbDetector, usb_detector_),
        zeroconf_detector_(zeroconf_detector),
        zeroconf_detector_observer_proxy_(this,
                                          kZeroconfDetector,
                                          zeroconf_detector_),
        ppd_provider_(std::move(ppd_provider)),
        printers_(kNumPrinterClasses),
        weak_ptr_factory_(this) {
    synced_printers_manager_observer_.Add(synced_printers_manager_);
  }

  ~CupsPrintersManagerImpl() override = default;

  // Public API function.
  std::vector<Printer> GetPrinters(PrinterClass printer_class) const override {
    return printers_.at(printer_class);
  }

  // Public API function.
  void RemoveUnavailablePrinters(
      std::vector<Printer>* printers) const override {
    FilterOutPrinters(printers, [this](const Printer& printer) {
      return !PrinterAvailable(printer);
    });
  }

  // Public API function.
  void UpdateConfiguredPrinter(const Printer& printer) override {
    synced_printers_manager_->UpdateConfiguredPrinter(printer);
  }

  // Public API function.
  void RemoveConfiguredPrinter(const std::string& printer_id) override {
    synced_printers_manager_->RemoveConfiguredPrinter(printer_id);
  }

  // Public API function.
  void AddObserver(CupsPrintersManager::Observer* observer) override {
    observer_list_.AddObserver(observer);
  }

  // Public API function.
  void RemoveObserver(CupsPrintersManager::Observer* observer) override {
    observer_list_.RemoveObserver(observer);
  }

  // Public API function.
  void PrinterInstalled(const Printer& printer) override {
    synced_printers_manager_->PrinterInstalled(printer);
  }

  // Public API function.
  bool IsPrinterInstalled(const Printer& printer) const override {
    return synced_printers_manager_->IsConfigurationCurrent(printer);
  }

  // Public API function.
  // Note this is linear in the number of printers.  If the number of printers
  // gets so large that a linear search is prohibative, we'll have to rethink
  // more than just this function.
  std::unique_ptr<Printer> GetPrinter(const std::string& id) const override {
    for (const auto& printer_list : printers_) {
      for (const auto& printer : printer_list) {
        if (printer.id() == id) {
          return base::MakeUnique<Printer>(printer);
        }
      }
    }
    return std::unique_ptr<Printer>();
  }

  // SyncedPrintersManager::Observer implementation
  void OnConfiguredPrintersChanged(
      const std::vector<Printer>& printers) override {
    printers_[kConfigured] = printers;
    configured_printer_ids_ = GetIdsSet(printers);
    for (auto& observer : observer_list_) {
      observer.OnPrintersChanged(kConfigured, printers_[kConfigured]);
    }
    RebuildDetectedLists();
  }

  // SyncedPrintersManager::Observer implementation
  void OnEnterprisePrintersChanged(
      const std::vector<Printer>& printers) override {
    printers_[kEnterprise] = printers;
    for (auto& observer : observer_list_) {
      observer.OnPrintersChanged(kEnterprise, printers_[kEnterprise]);
    }
  }

  // Callback entry point for PrinterDetectorObserverProxys owned by this
  // object.
  void OnPrintersFound(
      int detector_id,
      const std::vector<PrinterDetector::DetectedPrinter>& printers) {
    switch (detector_id) {
      case kUsbDetector:
        usb_detections_ = printers;
        break;
      case kZeroconfDetector:
        zeroconf_detections_ = printers;
        break;
    }
    RebuildDetectedLists();
  }

 private:
  // Return whether or not we believe this printer is currently available for
  // printing.  This is not a perfect test -- we just assume any IPP printers
  // are available because, in cases where there are a large number of
  // printers available, probing IPP printers would generate too much network
  // spam.  This is intended to help filter out local printers that are not
  // available (USB, zeroconf, ...)
  //
  // TODO(justincarlson) - Implement this.  Until it's implemented, we'll never
  // filter out unavailable printers from potential printer targets.  While
  // suboptimal, this is ok--it just means that we will fail to print if the
  // user selects a printer that's not available.
  bool PrinterAvailable(const Printer& printer) const { return true; }

  void AddDetectedList(
      const std::vector<PrinterDetector::DetectedPrinter>& detected_list) {
    for (const PrinterDetector::DetectedPrinter& detected : detected_list) {
      if (base::ContainsKey(configured_printer_ids_, detected.printer.id())) {
        // It's already in the configured classes, so neither automatic nor
        // discovered is appropriate.  Skip it.
        continue;
      }
      auto it = detected_printer_ppd_references_.find(detected.printer.id());
      if (it != detected_printer_ppd_references_.end()) {
        if (it->second == nullptr) {
          // We couldn't figure out this printer, so it's in the discovered
          // class.
          printers_[kDiscovered].push_back(detected.printer);
        } else {
          // We have a ppd reference, so we think we can set this up
          // automatically.
          printers_[kAutomatic].push_back(detected.printer);
          *printers_[kAutomatic].back().mutable_ppd_reference() = *it->second;
        }
      } else {
        // Didn't find an entry for this printer in the PpdReferences cache.
        // We need to ask PpdProvider whether or not it can determine a
        // PpdReference.  If there's not already an outstanding request for
        // one, start one.  When the request comes back, we'll rerun
        // classification and then should be able to figure out where this
        // printer belongs.
        if (!base::ContainsKey(inflight_ppd_reference_resolutions_,
                               detected.printer.id())) {
          inflight_ppd_reference_resolutions_.insert(detected.printer.id());
          ppd_provider_->ResolvePpdReference(
              detected.ppd_search_data,
              base::Bind(&CupsPrintersManagerImpl::ResolvePpdReferenceDone,
                         weak_ptr_factory_.GetWeakPtr(),
                         detected.printer.id()));
        }
      }
    }
  }

  // Rebuild the Automatic and Discovered printers lists.
  void RebuildDetectedLists() {
    printers_[kAutomatic].clear();
    printers_[kDiscovered].clear();
    AddDetectedList(usb_detections_);
    AddDetectedList(zeroconf_detections_);

    for (auto& observer : observer_list_) {
      observer.OnPrintersChanged(kAutomatic, printers_[kAutomatic]);
      observer.OnPrintersChanged(kDiscovered, printers_[kDiscovered]);
    }
  }

  // Callback invoked on completion of PpdProvider::ResolvePpdReference.
  void ResolvePpdReferenceDone(const std::string& printer_id,
                               PpdProvider::CallbackResultCode code,
                               const Printer::PpdReference& ref) {
    inflight_ppd_reference_resolutions_.erase(printer_id);
    // Create the entry.
    std::unique_ptr<Printer::PpdReference>& value =
        detected_printer_ppd_references_[printer_id];
    if (code == PpdProvider::SUCCESS) {
      // If we got something, populate the entry.  Otherwise let it
      // just remain null.
      value.reset(new Printer::PpdReference(ref));
    }
    RebuildDetectedLists();
  }

  // Source lists for detected printers.
  std::vector<PrinterDetector::DetectedPrinter> usb_detections_;
  std::vector<PrinterDetector::DetectedPrinter> zeroconf_detections_;

  // Not owned.
  SyncedPrintersManager* synced_printers_manager_;
  ScopedObserver<SyncedPrintersManager, SyncedPrintersManager::Observer>
      synced_printers_manager_observer_;

  // Not owned.
  PrinterDetector* usb_detector_;
  PrinterDetectorObserverProxy usb_detector_observer_proxy_;

  // Not owned.
  PrinterDetector* zeroconf_detector_;
  PrinterDetectorObserverProxy zeroconf_detector_observer_proxy_;

  scoped_refptr<PpdProvider> ppd_provider_;

  // Categorized printers.  This is indexed by PrinterClass.
  std::vector<std::vector<Printer>> printers_;

  // Printer ids that occur in one of our categories or printers.
  std::unordered_set<std::string> known_printer_ids_;

  // This is a dual-purpose structure.  The keys in the map are printer ids.
  // If an entry exists in this map it means we have received a response from
  // PpdProvider about a PpdReference for the given printer.  A null value
  // means we don't have a PpdReference (and so can't set up this printer
  // automatically).
  std::unordered_map<std::string, std::unique_ptr<Printer::PpdReference>>
      detected_printer_ppd_references_;

  // Printer ids for which we have sent off a request to PpdProvider for a ppd
  // reference, but have not yet gotten a response.
  std::unordered_set<std::string> inflight_ppd_reference_resolutions_;

  // Ids of all printers in the configured class.
  std::unordered_set<std::string> configured_printer_ids_;

  base::ObserverList<CupsPrintersManager::Observer> observer_list_;
  base::WeakPtrFactory<CupsPrintersManagerImpl> weak_ptr_factory_;
};

void PrinterDetectorObserverProxy::OnPrintersFound(
    const std::vector<PrinterDetector::DetectedPrinter>& printers) {
  parent_->OnPrintersFound(id_, printers);
}

// Placeholder stubbed out detector implementation for Zeroconf detection (the
// actual implementation hasn't landed yet).  This stub has no state and
// never detects anything.
class ZeroconfDetectorStub : public PrinterDetector {
 public:
  ~ZeroconfDetectorStub() override {}
  void Start() override {}
  void AddObserver(Observer* observer) override {}
  void RemoveObserver(Observer* observer) override {}
  std::vector<DetectedPrinter> GetPrinters() override {
    return std::vector<DetectedPrinter>();
  }

  // Since ZeroconfDetector will not be owned by CupsPrintersManager, the
  // easiest thing to do for now is to make the stub a singleton.
  static ZeroconfDetectorStub* GetInstance() {
    return base::Singleton<ZeroconfDetectorStub>::get();
  }
};

}  // namespace

// static
std::unique_ptr<CupsPrintersManager> CupsPrintersManager::Create(
    Profile* profile) {
  return base::MakeUnique<CupsPrintersManagerImpl>(
      SyncedPrintersManagerFactory::GetInstance()->GetForBrowserContext(
          profile),
      UsbPrinterDetectorFactory::GetInstance()->Get(profile),
      ZeroconfDetectorStub::GetInstance(), CreatePpdProvider(profile));
}

// static
std::unique_ptr<CupsPrintersManager> CupsPrintersManager::Create(
    SyncedPrintersManager* synced_printers_manager,
    PrinterDetector* usb_detector,
    PrinterDetector* zeroconf_detector,
    scoped_refptr<PpdProvider> ppd_provider) {
  return base::MakeUnique<CupsPrintersManagerImpl>(
      synced_printers_manager, usb_detector, zeroconf_detector,
      std::move(ppd_provider));
}

}  // namespace chromeos
