// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/synced_printers_manager.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/guid.h"
#include "base/json/json_reader.h"
#include "base/md5.h"
#include "base/memory/ptr_util.h"
#include "base/optional.h"
#include "base/values.h"
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

// Adds |printer| with |id| to prefs.  Returns true if the printer is new,
// false for an update.
bool UpdatePrinterPref(PrintersSyncBridge* sync_bridge,
                       const std::string& id,
                       const Printer& printer) {
  base::Optional<sync_pb::PrinterSpecifics> specifics =
      sync_bridge->GetPrinter(id);
  if (!specifics.has_value()) {
    sync_bridge->AddPrinter(PrinterToSpecifics(printer));
    return true;
  }

  // Preserve fields in the proto which we don't understand.
  std::unique_ptr<sync_pb::PrinterSpecifics> updated_printer =
      base::MakeUnique<sync_pb::PrinterSpecifics>(*specifics);
  MergePrinterToSpecifics(printer, updated_printer.get());
  sync_bridge->AddPrinter(std::move(updated_printer));

  return false;
}

class SyncedPrintersManagerImpl : public SyncedPrintersManager {
 public:
  SyncedPrintersManagerImpl(Profile* profile,
                            std::unique_ptr<PrintersSyncBridge> sync_bridge)
      : profile_(profile), sync_bridge_(std::move(sync_bridge)) {
    pref_change_registrar_.Init(profile->GetPrefs());
    pref_change_registrar_.Add(
        prefs::kRecommendedNativePrinters,
        base::Bind(&SyncedPrintersManagerImpl::UpdateRecommendedPrinters,
                   base::Unretained(this)));
    UpdateRecommendedPrinters();
  }
  ~SyncedPrintersManagerImpl() override = default;

  std::vector<Printer> GetConfiguredPrinters() const override {
    std::vector<Printer> printers;
    std::vector<sync_pb::PrinterSpecifics> values =
        sync_bridge_->GetAllPrinters();
    for (const auto& value : values) {
      printers.push_back(*SpecificsToPrinter(value));
    }
    return printers;
  }

  std::vector<Printer> GetEnterprisePrinters() const override {
    std::vector<Printer> printers;
    for (const std::string& key : enterprise_printer_ids_) {
      auto printer = enterprise_printers_.find(key);
      if (printer != enterprise_printers_.end()) {
        printers.push_back(*printer->second);
      }
    }
    return printers;
  }

  std::unique_ptr<Printer> GetPrinter(
      const std::string& printer_id) const override {
    // check for a policy printer first
    const auto& policy_printers = enterprise_printers_;
    auto found = policy_printers.find(printer_id);
    if (found != policy_printers.end()) {
      // Copy a printer.
      return base::MakeUnique<Printer>(*(found->second));
    }

    base::Optional<sync_pb::PrinterSpecifics> printer =
        sync_bridge_->GetPrinter(printer_id);
    return printer.has_value() ? SpecificsToPrinter(*printer) : nullptr;
  }

  void UpdateConfiguredPrinter(const Printer& printer_arg) override {
    // Need a local copy since we may set the id.
    Printer printer = printer_arg;
    if (printer.id().empty()) {
      printer.set_id(base::GenerateGUID());
    }

    DCHECK_EQ(Printer::SRC_USER_PREFS, printer.source());
    UpdatePrinterPref(sync_bridge_.get(), printer.id(), printer);

    NotifyConfiguredObservers();
  }

  bool RemoveConfiguredPrinter(const std::string& printer_id) override {
    DCHECK(!printer_id.empty());

    base::Optional<sync_pb::PrinterSpecifics> printer =
        sync_bridge_->GetPrinter(printer_id);
    bool success = false;
    if (printer.has_value()) {
      std::unique_ptr<Printer> p = SpecificsToPrinter(*printer);
      success = sync_bridge_->RemovePrinter(p->id());
      if (success) {
        NotifyConfiguredObservers();
      }
    } else {
      LOG(WARNING) << "Could not find printer" << printer_id;
    }

    return success;
  }

  void AddObserver(Observer* observer) override {
    observers_.AddObserver(observer);
  }

  void RemoveObserver(Observer* observer) override {
    observers_.RemoveObserver(observer);
  }

  void PrinterInstalled(const Printer& printer) override {
    DCHECK(!printer.last_updated().is_null());
    installed_printer_timestamps_[printer.id()] = printer.last_updated();
  }

  bool IsConfigurationCurrent(const Printer& printer) const override {
    auto found = installed_printer_timestamps_.find(printer.id());
    if (found == installed_printer_timestamps_.end())
      return false;

    return found->second == printer.last_updated();
  }

  PrintersSyncBridge* GetSyncBridge() override { return sync_bridge_.get(); }

 private:
  // This method is not thread safe and could interact poorly with readers of
  // |enterprise_printers_|.
  void UpdateRecommendedPrinters() {
    const PrefService* prefs = profile_->GetPrefs();

    const base::ListValue* values =
        prefs->GetList(prefs::kRecommendedNativePrinters);
    const base::Time timestamp = base::Time::Now();

    // Parse the policy JSON into new structures.
    std::vector<std::string> new_ids;
    std::map<std::string, std::unique_ptr<Printer>> new_printers;
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
      // unique so we'll hash the record.  This will not collide with the UUIDs
      // generated for user entries.
      std::string id = base::MD5String(printer_json);
      printer_dictionary->SetString(kPrinterId, id);

      if (base::ContainsKey(new_printers, id)) {
        // Skip duplicated entries.
        LOG(WARNING) << "Duplicate printer ignored.";
        continue;
      }

      new_ids.push_back(id);
      // Move existing printers, create othewise.
      auto old = enterprise_printers_.find(id);
      if (old != enterprise_printers_.end()) {
        new_printers[id] = std::move(old->second);
      } else {
        auto printer =
            RecommendedPrinterToPrinter(*printer_dictionary, timestamp);
        printer->set_source(Printer::SRC_POLICY);

        new_printers[id] = std::move(printer);
      }
    }

    // Objects not in the most recent update get deallocated after method exit.
    enterprise_printer_ids_.swap(new_ids);
    enterprise_printers_.swap(new_printers);
    NotifyEnterpriseObservers();
  }

  // Notify observers of a change in the set of Configured printers.
  void NotifyConfiguredObservers() {
    std::vector<Printer> printers = GetConfiguredPrinters();
    for (Observer& obs : observers_) {
      obs.OnConfiguredPrintersChanged(printers);
    }
  }

  // Notify observers of a change in the set of Enterprise printers.
  void NotifyEnterpriseObservers() {
    std::vector<Printer> printers = GetEnterprisePrinters();
    for (Observer& obs : observers_) {
      obs.OnEnterprisePrintersChanged(printers);
    }
  }

 private:
  Profile* profile_;
  PrefChangeRegistrar pref_change_registrar_;

  // The backend for profile printers.
  std::unique_ptr<PrintersSyncBridge> sync_bridge_;

  // Contains the keys for all enterprise printers in order so we can return
  // the list of enterprise printers in the order they were received.
  std::vector<std::string> enterprise_printer_ids_;
  std::map<std::string, std::unique_ptr<Printer>> enterprise_printers_;

  // Map of printer ids to installation timestamps.
  std::map<std::string, base::Time> installed_printer_timestamps_;

  base::ObserverList<Observer> observers_;
};

}  // namespace

// static
void SyncedPrintersManager::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterListPref(prefs::kPrintingDevices,
                             user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterListPref(prefs::kRecommendedNativePrinters);
}

// static
std::unique_ptr<SyncedPrintersManager> SyncedPrintersManager::Create(
    Profile* profile,
    std::unique_ptr<PrintersSyncBridge> sync_bridge) {
  return base::MakeUnique<SyncedPrintersManagerImpl>(profile,
                                                     std::move(sync_bridge));
}

}  // namespace chromeos
