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

#include "base/bind.h"
#include "base/memory/singleton.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "base/scoped_observer.h"
#include "base/sequence_checker.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/chromeos/printing/ppd_provider_factory.h"
#include "chrome/browser/chromeos/printing/ppd_resolution_tracker.h"
#include "chrome/browser/chromeos/printing/printer_event_tracker_factory.h"
#include "chrome/browser/chromeos/printing/synced_printers_manager.h"
#include "chrome/browser/chromeos/printing/synced_printers_manager_factory.h"
#include "chrome/browser/chromeos/printing/usb_printer_detector.h"
#include "chrome/browser/chromeos/printing/zeroconf_printer_detector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/policy/policy_constants.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_member.h"
#include "components/prefs/pref_service.h"

namespace chromeos {
namespace {

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
                          std::unique_ptr<PrinterDetector> usb_detector,
                          std::unique_ptr<PrinterDetector> zeroconf_detector,
                          scoped_refptr<PpdProvider> ppd_provider,
                          PrinterEventTracker* event_tracker,
                          PrefService* pref_service)
      : synced_printers_manager_(synced_printers_manager),
        synced_printers_manager_observer_(this),
        usb_detector_(std::move(usb_detector)),
        zeroconf_detector_(std::move(zeroconf_detector)),
        ppd_provider_(std::move(ppd_provider)),
        event_tracker_(event_tracker),
        printers_(kNumPrinterClasses),
        weak_ptr_factory_(this) {
    // Prime the printer cache with the saved and enterprise printers.
    printers_[kSaved] = synced_printers_manager_->GetSavedPrinters();
    RebuildConfiguredPrintersIndex();
    synced_printers_manager_observer_.Add(synced_printers_manager_);
    enterprise_printers_are_ready_ =
        synced_printers_manager_->GetEnterprisePrinters(
            &(printers_[kEnterprise]));

    // Callbacks may ensue immediately when the observer proxies are set up, so
    // these instantiations must come after everything else is initialized.
    usb_detector_->RegisterPrintersFoundCallback(
        base::BindRepeating(&CupsPrintersManagerImpl::OnPrintersFound,
                            weak_ptr_factory_.GetWeakPtr(), kUsbDetector));
    OnPrintersFound(kUsbDetector, usb_detector_->GetPrinters());

    zeroconf_detector_->RegisterPrintersFoundCallback(
        base::BindRepeating(&CupsPrintersManagerImpl::OnPrintersFound,
                            weak_ptr_factory_.GetWeakPtr(), kZeroconfDetector));
    OnPrintersFound(kZeroconfDetector, zeroconf_detector_->GetPrinters());

    native_printers_allowed_.Init(prefs::kUserNativePrintersAllowed,
                                  pref_service);
    send_username_and_filename_.Init(
        prefs::kPrintingSendUsernameAndFilenameEnabled, pref_service);
  }

  ~CupsPrintersManagerImpl() override = default;

  // Public API function.
  std::vector<Printer> GetPrinters(PrinterClass printer_class) const override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
    if (!native_printers_allowed_.GetValue() && printer_class != kEnterprise) {
      // If native printers are disabled then simply return an empty vector.
      LOG(WARNING) << "Attempting to retrieve native printers when "
                      "UserNativePrintersAllowed is set to false";
      return {};
    }
    if (send_username_and_filename_.GetValue()) {
      std::vector<Printer> result(printers_[printer_class].size());
      auto it_end = std::copy_if(
          printers_[printer_class].begin(), printers_[printer_class].end(),
          result.begin(), [](const Printer& printer) {
            return !printer.HasNetworkProtocol() ||
                   printer.GetProtocol() == Printer::kIpps;
          });
      result.resize(it_end - result.begin());
      return result;
    }
    return printers_.at(printer_class);
  }

  // Public API function.
  void RemoveUnavailablePrinters(
      std::vector<Printer>* printers) const override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
    FilterOutPrinters(printers, [this](const Printer& printer) {
      return !PrinterAvailable(printer);
    });
  }

  // Public API function.
  void UpdateSavedPrinter(const Printer& printer) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
    if (!native_printers_allowed_.GetValue()) {
      LOG(WARNING) << "UpdateSavedPrinter() called when "
                      "UserNativePrintersAllowed is set to false";
      return;
    }
    // If this is an 'add' instead of just an update, record the event.
    MaybeRecordInstallation(printer, false /* is_automatic_installation */);
    synced_printers_manager_->UpdateSavedPrinter(printer);
    // Note that we will rebuild our lists when we get the observer
    // callback from |synced_printers_manager_|.
  }

  // Public API function.
  void RemoveSavedPrinter(const std::string& printer_id) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
    auto existing = synced_printers_manager_->GetPrinter(printer_id);
    if (existing) {
      event_tracker_->RecordPrinterRemoved(*existing);
    }
    synced_printers_manager_->RemoveSavedPrinter(printer_id);
    // Note that we will rebuild our lists when we get the observer
    // callback from |synced_printers_manager_|.
  }

  // Public API function.
  void AddObserver(CupsPrintersManager::Observer* observer) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
    observer_list_.AddObserver(observer);
    if (enterprise_printers_are_ready_) {
      observer->OnEnterprisePrintersInitialized();
    }
  }

  // Public API function.
  void RemoveObserver(CupsPrintersManager::Observer* observer) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
    observer_list_.RemoveObserver(observer);
  }

  // Public API function.
  void PrinterInstalled(const Printer& printer, bool is_automatic) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
    if (!native_printers_allowed_.GetValue()) {
      LOG(WARNING) << "PrinterInstalled() called when "
                      "UserNativePrintersAllowed is  set to false";
      return;
    }
    MaybeRecordInstallation(printer, is_automatic);
    synced_printers_manager_->PrinterInstalled(printer);
  }

  // Public API function.
  bool IsPrinterInstalled(const Printer& printer) const override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
    return synced_printers_manager_->IsConfigurationCurrent(printer);
  }

  // Public API function.
  // Note this is linear in the number of printers.  If the number of printers
  // gets so large that a linear search is prohibative, we'll have to rethink
  // more than just this function.
  base::Optional<Printer> GetPrinter(const std::string& id) const override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
    if (!native_printers_allowed_.GetValue()) {
      LOG(WARNING) << "UserNativePrintersAllowed is disabled - only searching "
                      "enterprise printers";
      return GetEnterprisePrinter(id);
    }

    for (const auto& printer_list : printers_) {
      for (const auto& printer : printer_list) {
        if (printer.id() == id) {
          return printer;
        }
      }
    }
    return base::nullopt;
  }

  // SyncedPrintersManager::Observer implementation
  void OnSavedPrintersChanged() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
    printers_[kSaved] = synced_printers_manager_->GetSavedPrinters();
    RebuildConfiguredPrintersIndex();
    RebuildDetectedLists();
    NotifyObservers({kSaved});
  }

  // SyncedPrintersManager::Observer implementation
  void OnEnterprisePrintersChanged() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
    const bool enterprise_printers_are_ready =
        synced_printers_manager_->GetEnterprisePrinters(
            &(printers_[kEnterprise]));
    if (enterprise_printers_are_ready && !enterprise_printers_are_ready_) {
      enterprise_printers_are_ready_ = true;
      for (auto& observer : observer_list_) {
        observer.OnEnterprisePrintersInitialized();
      }
    }
    NotifyObservers({kEnterprise});
  }

  // Callback for PrinterDetectors.
  void OnPrintersFound(
      int detector_id,
      const std::vector<PrinterDetector::DetectedPrinter>& printers) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
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
  base::Optional<Printer> GetEnterprisePrinter(const std::string& id) const {
    for (const auto& printer : printers_[kEnterprise]) {
      if (printer.id() == id) {
        return printer;
      }
    }
    return base::nullopt;
  }

  // Notify observers on the given classes the the relevant lists have changed.
  void NotifyObservers(
      const std::vector<CupsPrintersManager::PrinterClass>& printer_classes) {
    for (auto& observer : observer_list_) {
      for (auto printer_class : printer_classes) {
        observer.OnPrintersChanged(printer_class, printers_[printer_class]);
      }
    }
  }

  // Rebuild the index from printer id to index for configured printers.
  // TODO(baileyberro): Update ConfiguredPrintersIndex to support saved and
  // unsaved printers, since either could need an updated URI.
  void RebuildConfiguredPrintersIndex() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
    configured_printers_index_.clear();
    for (size_t i = 0; i < printers_[kSaved].size(); ++i) {
      configured_printers_index_[printers_[kSaved][i].id()] = i;
    }
  }

  // Look through all sources for the detected printer with the given id.
  // Return a pointer to the printer on found, null if no entry is found.
  const PrinterDetector::DetectedPrinter* FindDetectedPrinter(
      const std::string& id) const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
    for (const auto* printer_list : {&usb_detections_, &zeroconf_detections_}) {
      for (const auto& detected : *printer_list) {
        if (detected.printer.id() == id) {
          return &detected;
        }
      }
    }
    return nullptr;
  }

  void MaybeRecordInstallation(const Printer& printer,
                               bool is_automatic_installation) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
    if (synced_printers_manager_->GetPrinter(printer.id())) {
      // It's just an update, not a new installation, so don't record an event.
      return;
    }

    // For compatibility with the previous implementation, record USB printers
    // separately from other IPP printers.  Eventually we may want to shift
    // this to be split by autodetected/not autodetected instead of USB/other
    // IPP.
    if (printer.IsUsbProtocol()) {
      // Get the associated detection record if one exists.
      const auto* detected = FindDetectedPrinter(printer.id());
      // We should have the full DetectedPrinter.  We can't log the printer if
      // we don't have it.
      if (!detected) {
        LOG(WARNING) << "Failed to find USB printer " << printer.id()
                     << " for installation event logging";
        return;
      }
      // For recording purposes, this is an automatic install if the ppd
      // reference generated at detection time is the is the one we actually
      // used -- i.e. the user didn't have to change anything to obtain a ppd
      // that worked.
      PrinterEventTracker::SetupMode mode;
      if (is_automatic_installation) {
        mode = PrinterEventTracker::kAutomatic;
      } else {
        mode = PrinterEventTracker::kUser;
      }
      event_tracker_->RecordUsbPrinterInstalled(*detected, mode);
    } else {
      PrinterEventTracker::SetupMode mode;
      if (is_automatic_installation) {
        mode = PrinterEventTracker::kAutomatic;
      } else {
        mode = PrinterEventTracker::kUser;
      }
      event_tracker_->RecordIppPrinterInstalled(printer, mode);
    }
  }

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
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
    for (const PrinterDetector::DetectedPrinter& detected : detected_list) {
      const std::string& detected_printer_id = detected.printer.id();

      if (base::ContainsKey(configured_printers_index_, detected_printer_id)) {
        // It's already in the configured class, don't need to do anything
        // else here.
        continue;
      }

      // Sometimes the detector can flag a printer as IPP-everywhere compatible;
      // those printers can go directly into the automatic class without further
      // processing.
      if (detected.printer.IsIppEverywhere()) {
        printers_[kAutomatic].push_back(detected.printer);
        continue;
      }
      if (ppd_resolution_tracker_.IsResolutionComplete(detected_printer_id)) {
        if (!ppd_resolution_tracker_.WasResolutionSuccessful(
                detected_printer_id)) {
          auto printer = detected.printer;
          if (!printer.supports_ippusb()) {
            // We couldn't figure out this printer, so it's in the discovered
            // class.
            printers_[kDiscovered].push_back(printer);
            continue;
          }
          // If the detected printer supports ipp-over-usb and we could not find
          // a ppd for it, then we switch to the ippusb scheme and mark it as
          // autoconf.
          printer.set_uri(
              base::StringPrintf("ippusb://%04x_%04x/ipp/print",
                                 detected.ppd_search_data.usb_vendor_id,
                                 detected.ppd_search_data.usb_product_id));
          printer.mutable_ppd_reference()->autoconf = true;
          printers_[kAutomatic].push_back(printer);
        } else {
          // We have a ppd reference, so we think we can set this up
          // automatically.
          printers_[kAutomatic].push_back(detected.printer);
          *printers_[kAutomatic].back().mutable_ppd_reference() =
              ppd_resolution_tracker_.GetPpdReference(detected_printer_id);
        }
      } else {
        // Didn't find an entry for this printer in the PpdReferences cache.  We
        // need to ask PpdProvider whether or not it can determine a
        // PpdReference.  If there's not already an outstanding request for one,
        // start one.  When the request comes back, we'll rerun classification
        // and then should be able to figure out where this printer belongs.

        if (!ppd_resolution_tracker_.IsResolutionPending(detected_printer_id)) {
          ppd_resolution_tracker_.MarkResolutionPending(detected_printer_id);
          ppd_provider_->ResolvePpdReference(
              detected.ppd_search_data,
              base::Bind(&CupsPrintersManagerImpl::ResolvePpdReferenceDone,
                         weak_ptr_factory_.GetWeakPtr(), detected_printer_id));
        }
      }
    }
  }

  // Record in UMA the appropriate event with a setup attempt for a printer is
  // abandoned.
  void RecordSetupAbandoned(const Printer& printer) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
    if (printer.IsUsbProtocol()) {
      const auto* detected = FindDetectedPrinter(printer.id());
      if (!detected) {
        LOG(WARNING) << "Failed to find USB printer " << printer.id()
                     << " for abandoned event logging";
        return;
      }
      event_tracker_->RecordUsbSetupAbandoned(*detected);
    } else {
      event_tracker_->RecordSetupAbandoned(printer);
    }
  }

  // Rebuild the Automatic and Discovered printers lists from the (cached) raw
  // detections.  This will also generate OnPrintersChanged events for any
  // observers observering either of the detected lists (kAutomatic and
  // kDiscovered).
  void RebuildDetectedLists() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
    printers_[kAutomatic].clear();
    printers_[kDiscovered].clear();
    AddDetectedList(usb_detections_);
    AddDetectedList(zeroconf_detections_);
    NotifyObservers({kAutomatic, kDiscovered});
  }

  // Callback invoked on completion of PpdProvider::ResolvePpdReference.
  void ResolvePpdReferenceDone(const std::string& printer_id,
                               PpdProvider::CallbackResultCode code,
                               const Printer::PpdReference& ref) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
    if (code == PpdProvider::SUCCESS) {
      ppd_resolution_tracker_.MarkResolutionSuccessful(printer_id, ref);
    } else {
      ppd_resolution_tracker_.MarkResolutionFailed(printer_id);
    }
    RebuildDetectedLists();
  }

  SEQUENCE_CHECKER(sequence_);

  // Source lists for detected printers.
  std::vector<PrinterDetector::DetectedPrinter> usb_detections_;
  std::vector<PrinterDetector::DetectedPrinter> zeroconf_detections_;

  // Not owned.
  SyncedPrintersManager* const synced_printers_manager_;
  ScopedObserver<SyncedPrintersManager, SyncedPrintersManager::Observer>
      synced_printers_manager_observer_;

  std::unique_ptr<PrinterDetector> usb_detector_;

  std::unique_ptr<PrinterDetector> zeroconf_detector_;

  scoped_refptr<PpdProvider> ppd_provider_;

  // Not owned
  PrinterEventTracker* const event_tracker_;

  // Categorized printers.  This is indexed by PrinterClass.
  std::vector<std::vector<Printer>> printers_;

  // Equals true if the list of enterprise printers and related policies
  // is initialized and configured correctly.
  bool enterprise_printers_are_ready_ = false;

  // Printer ids that occur in one of our categories or printers.
  std::unordered_set<std::string> known_printer_ids_;

  // Tracks PpdReference resolution. Also stores USB manufacturer string if
  // available.
  PpdResolutionTracker ppd_resolution_tracker_;

  // Map from printer id to printers_[kSaved] index for configured
  // printers.
  std::unordered_map<std::string, int> configured_printers_index_;

  base::ObserverList<CupsPrintersManager::Observer>::Unchecked observer_list_;

  // Holds the current value of the pref |UserNativePrintersAllowed|.
  BooleanPrefMember native_printers_allowed_;

  // Holds the current value of the pref
  // |PrintingSendUsernameAndFilenameEnabled|.
  BooleanPrefMember send_username_and_filename_;

  base::WeakPtrFactory<CupsPrintersManagerImpl> weak_ptr_factory_;
};

}  // namespace

// static
std::unique_ptr<CupsPrintersManager> CupsPrintersManager::Create(
    Profile* profile) {
  return std::make_unique<CupsPrintersManagerImpl>(
      SyncedPrintersManagerFactory::GetInstance()->GetForBrowserContext(
          profile),
      UsbPrinterDetector::Create(), ZeroconfPrinterDetector::Create(),
      CreatePpdProvider(profile),
      PrinterEventTrackerFactory::GetInstance()->GetForBrowserContext(profile),
      profile->GetPrefs());
}

// static
std::unique_ptr<CupsPrintersManager> CupsPrintersManager::CreateForTesting(
    SyncedPrintersManager* synced_printers_manager,
    std::unique_ptr<PrinterDetector> usb_detector,
    std::unique_ptr<PrinterDetector> zeroconf_detector,
    scoped_refptr<PpdProvider> ppd_provider,
    PrinterEventTracker* event_tracker,
    PrefService* pref_service) {
  return std::make_unique<CupsPrintersManagerImpl>(
      synced_printers_manager, std::move(usb_detector),
      std::move(zeroconf_detector), std::move(ppd_provider), event_tracker,
      pref_service);
}

// static
void CupsPrintersManager::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(
      prefs::kUserNativePrintersAllowed, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(prefs::kPrintingSendUsernameAndFilenameEnabled,
                                false);
}

}  // namespace chromeos
