// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/synced_printers_manager.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/guid.h"
#include "base/json/json_reader.h"
#include "base/md5.h"
#include "base/memory/ptr_util.h"
#include "base/observer_list_threadsafe.h"
#include "base/optional.h"
#include "base/synchronization/lock.h"
#include "base/values.h"
#include "chrome/browser/chromeos/printing/printer_configurer.h"
#include "chrome/browser/chromeos/printing/printers_sync_bridge.h"
#include "chrome/browser/chromeos/printing/specifics_translation.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chromeos/printing/printer_configuration.h"
#include "chromeos/printing/printer_translator.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"

namespace chromeos {

namespace {

// Enumeration values for NativePrintersBulkAccessMode.
constexpr int kBlacklistAccess = 0;
// TODO(crbug.com/758680): Parse the access policy.
// constexpr int kWhitelistAccess = 1;
// constexpr int kAllAccess = 2;

class SyncedPrintersManagerImpl : public SyncedPrintersManager,
                                  public PrintersSyncBridge::Observer {
 public:
  SyncedPrintersManagerImpl(Profile* profile,
                            std::unique_ptr<PrintersSyncBridge> sync_bridge)
      : profile_(profile),
        sync_bridge_(std::move(sync_bridge)),
        observers_(new base::ObserverListThreadSafe<
                   SyncedPrintersManager::Observer>()),
        weak_factory_(this) {
    pref_change_registrar_.Init(profile->GetPrefs());
    pref_change_registrar_.Add(
        prefs::kRecommendedNativePrinters,
        base::Bind(&SyncedPrintersManagerImpl::UpdateRecommendedPrinters,
                   base::Unretained(this)));
    UpdateRecommendedPrinters();
    sync_bridge_->AddObserver(this);
  }

  ~SyncedPrintersManagerImpl() override { sync_bridge_->RemoveObserver(this); }

  std::vector<Printer> GetConfiguredPrinters() const override {
    // No need to lock here, since sync_bridge_ is thread safe and we don't
    // touch anything else.
    std::vector<Printer> printers;
    std::vector<sync_pb::PrinterSpecifics> values =
        sync_bridge_->GetAllPrinters();
    for (const auto& value : values) {
      printers.push_back(*SpecificsToPrinter(value));
    }
    return printers;
  }

  std::vector<Printer> GetEnterprisePrinters() const override {
    base::AutoLock l(lock_);
    return GetEnterprisePrintersLocked();
  }

  std::unique_ptr<Printer> GetPrinter(
      const std::string& printer_id) const override {
    base::AutoLock l(lock_);
    return GetPrinterLocked(printer_id);
  }

  void UpdateConfiguredPrinter(const Printer& printer) override {
    base::AutoLock l(lock_);
    UpdateConfiguredPrinterLocked(printer);
  }

  bool RemoveConfiguredPrinter(const std::string& printer_id) override {
    return sync_bridge_->RemovePrinter(printer_id);
  }

  void AddObserver(SyncedPrintersManager::Observer* observer) override {
    observers_->AddObserver(observer);
  }

  void RemoveObserver(SyncedPrintersManager::Observer* observer) override {
    observers_->RemoveObserver(observer);
  }

  void PrinterInstalled(const Printer& printer) override {
    base::AutoLock l(lock_);
    installed_printer_fingerprints_[printer.id()] =
        PrinterConfigurer::SetupFingerprint(printer);

    // Register this printer if it's the first time we're using it.
    if (GetPrinterLocked(printer.id()) == nullptr) {
      UpdateConfiguredPrinterLocked(printer);
    }
  }

  bool IsConfigurationCurrent(const Printer& printer) const override {
    base::AutoLock l(lock_);
    auto found = installed_printer_fingerprints_.find(printer.id());
    if (found == installed_printer_fingerprints_.end())
      return false;

    return found->second == PrinterConfigurer::SetupFingerprint(printer);
  }

  PrintersSyncBridge* GetSyncBridge() override { return sync_bridge_.get(); }

  // PrintersSyncBridge::Observer override.
  void OnPrintersUpdated() override {
    observers_->Notify(
        FROM_HERE,
        &SyncedPrintersManager::Observer::OnConfiguredPrintersChanged,
        GetConfiguredPrinters());
  }

 private:
  std::unique_ptr<Printer> GetPrinterLocked(
      const std::string& printer_id) const {
    lock_.AssertAcquired();
    // check for a policy printer first
    auto found = enterprise_printers_.find(printer_id);
    if (found != enterprise_printers_.end()) {
      // Copy a printer.
      return base::MakeUnique<Printer>(found->second);
    }

    base::Optional<sync_pb::PrinterSpecifics> printer =
        sync_bridge_->GetPrinter(printer_id);
    return printer.has_value() ? SpecificsToPrinter(*printer) : nullptr;
  }

  void UpdateConfiguredPrinterLocked(const Printer& printer_arg) {
    lock_.AssertAcquired();
    DCHECK_EQ(Printer::SRC_USER_PREFS, printer_arg.source());

    // Need a local copy since we may set the id.
    Printer printer = printer_arg;
    if (printer.id().empty()) {
      printer.set_id(base::GenerateGUID());
    }

    sync_bridge_->UpdatePrinter(PrinterToSpecifics(printer));
  }

  void UpdateRecommendedPrinters() {
    const PrefService* prefs = profile_->GetPrefs();

    const base::ListValue* values =
        prefs->GetList(prefs::kRecommendedNativePrinters);

    // Parse the policy JSON into new structures outside the lock.
    std::vector<std::string> new_ids;
    std::unordered_map<std::string, Printer> new_printers;

    new_ids.reserve(values->GetList().size());
    new_printers.reserve(values->GetList().size());
    for (const auto& value : *values) {
      std::string printer_json;
      if (!value.GetAsString(&printer_json)) {
        NOTREACHED();
        continue;
      }

      std::unique_ptr<base::DictionaryValue> printer_dictionary =
          base::DictionaryValue::From(base::JSONReader::Read(
              printer_json, base::JSON_ALLOW_TRAILING_COMMAS));

      if (!printer_dictionary) {
        LOG(WARNING) << "Ignoring invalid printer.  Invalid JSON object: "
                     << printer_json;
        continue;
      }

      // Policy printers don't have id's but the ids only need to be locally
      // unique so we'll hash the record.  This will not collide with the
      // UUIDs generated for user entries.
      std::string id = base::MD5String(printer_json);
      printer_dictionary->SetString(kPrinterId, id);

      if (base::ContainsKey(new_printers, id)) {
        // Skip duplicated entries.
        LOG(WARNING) << "Duplicate printer ignored.";
        continue;
      }
      new_ids.push_back(id);
      new_printers.insert(
          {id, *RecommendedPrinterToPrinter(*printer_dictionary)});
    }

    // Objects not in the most recent update get deallocated after method
    // exit.
    base::AutoLock l(lock_);
    enterprise_printer_ids_.swap(new_ids);
    enterprise_printers_.swap(new_printers);
    observers_->Notify(
        FROM_HERE,
        &SyncedPrintersManager::Observer::OnEnterprisePrintersChanged,
        GetEnterprisePrintersLocked());
  }

  std::vector<Printer> GetEnterprisePrintersLocked() const {
    lock_.AssertAcquired();
    std::vector<Printer> ret;
    ret.reserve(enterprise_printers_.size());
    for (const std::string& id : enterprise_printer_ids_) {
      ret.push_back(enterprise_printers_.find(id)->second);
    }
    return ret;
  }

  mutable base::Lock lock_;

  Profile* profile_;
  PrefChangeRegistrar pref_change_registrar_;

  // The backend for profile printers.
  std::unique_ptr<PrintersSyncBridge> sync_bridge_;

  // Enterprise printers as of the last time we got a policy update.  The ids
  // vector is used to preserve the received ordering.
  std::vector<std::string> enterprise_printer_ids_;
  // Map is from id to printer.
  std::unordered_map<std::string, Printer> enterprise_printers_;

  // Map of printer ids to PrinterConfigurer setup fingerprints at the time
  // the printers was last installed with CUPS.
  std::map<std::string, std::string> installed_printer_fingerprints_;

  scoped_refptr<base::ObserverListThreadSafe<SyncedPrintersManager::Observer>>
      observers_;
  base::WeakPtrFactory<SyncedPrintersManagerImpl> weak_factory_;
};

}  // namespace

// static
void SyncedPrintersManager::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterListPref(prefs::kPrintingDevices,
                             user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterListPref(prefs::kRecommendedNativePrinters);
  // Default value is blacklist.
  registry->RegisterIntegerPref(prefs::kRecommendedNativePrintersAccessMode,
                                kBlacklistAccess);
  registry->RegisterListPref(prefs::kRecommendedNativePrintersBlacklist);
  registry->RegisterListPref(prefs::kRecommendedNativePrintersWhitelist);
}

// static
std::unique_ptr<SyncedPrintersManager> SyncedPrintersManager::Create(
    Profile* profile,
    std::unique_ptr<PrintersSyncBridge> sync_bridge) {
  return base::MakeUnique<SyncedPrintersManagerImpl>(profile,
                                                     std::move(sync_bridge));
}

}  // namespace chromeos
