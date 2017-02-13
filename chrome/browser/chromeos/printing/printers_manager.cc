// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/printers_manager.h"

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
    sync_bridge->AddPrinter(printing::PrinterToSpecifics(printer));
    return true;
  }

  // Preserve fields in the proto which we don't understand.
  std::unique_ptr<sync_pb::PrinterSpecifics> updated_printer =
      base::MakeUnique<sync_pb::PrinterSpecifics>(*specifics);
  printing::MergePrinterToSpecifics(printer, updated_printer.get());
  sync_bridge->AddPrinter(std::move(updated_printer));

  return false;
}

}  // anonymous namespace

PrintersManager::PrintersManager(
    Profile* profile,
    std::unique_ptr<PrintersSyncBridge> sync_bridge)
    : profile_(profile), sync_bridge_(std::move(sync_bridge)) {
  pref_change_registrar_.Init(profile->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kRecommendedNativePrinters,
      base::Bind(&PrintersManager::UpdateRecommendedPrinters,
                 base::Unretained(this)));
  UpdateRecommendedPrinters();
}

PrintersManager::~PrintersManager() {}

// static
void PrintersManager::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterListPref(prefs::kPrintingDevices,
                             user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterListPref(prefs::kRecommendedNativePrinters);
}

std::vector<std::unique_ptr<Printer>> PrintersManager::GetPrinters() const {
  std::vector<std::unique_ptr<Printer>> printers;

  std::vector<sync_pb::PrinterSpecifics> values =
      sync_bridge_->GetAllPrinters();
  for (const auto& value : values) {
    printers.push_back(printing::SpecificsToPrinter(value));
  }

  return printers;
}

std::vector<std::unique_ptr<Printer>> PrintersManager::GetRecommendedPrinters()
    const {
  std::vector<std::unique_ptr<Printer>> printers;

  for (const std::string& key : recommended_printer_ids_) {
    const base::DictionaryValue& printer_dictionary =
        *(recommended_printers_.at(key));
    std::unique_ptr<Printer> printer =
        printing::RecommendedPrinterToPrinter(printer_dictionary);
    if (printer) {
      printer->set_source(Printer::SRC_POLICY);
      printers.push_back(std::move(printer));
    }
  }

  return printers;
}

std::unique_ptr<Printer> PrintersManager::GetPrinter(
    const std::string& printer_id) const {
  // check for a policy printer first
  const auto& policy_printers = recommended_printers_;
  auto found = policy_printers.find(printer_id);
  if (found != policy_printers.end())
    return printing::RecommendedPrinterToPrinter(*(found->second));

  base::Optional<sync_pb::PrinterSpecifics> printer =
      sync_bridge_->GetPrinter(printer_id);
  return printer.has_value() ? printing::SpecificsToPrinter(*printer) : nullptr;
}

void PrintersManager::RegisterPrinter(std::unique_ptr<Printer> printer) {
  if (printer->id().empty()) {
    printer->set_id(base::GenerateGUID());
  }

  DCHECK_EQ(Printer::SRC_USER_PREFS, printer->source());
  bool new_printer =
      UpdatePrinterPref(sync_bridge_.get(), printer->id(), *printer);

  if (new_printer) {
    for (Observer& obs : observers_) {
      obs.OnPrinterAdded(*printer);
    }
  } else {
    for (Observer& obs : observers_) {
      obs.OnPrinterUpdated(*printer);
    }
  }
}

bool PrintersManager::RemovePrinter(const std::string& printer_id) {
  DCHECK(!printer_id.empty());

  base::Optional<sync_pb::PrinterSpecifics> printer =
      sync_bridge_->GetPrinter(printer_id);
  bool success = false;
  if (printer.has_value()) {
    std::unique_ptr<Printer> p = printing::SpecificsToPrinter(*printer);
    success = sync_bridge_->RemovePrinter(p->id());
    if (success) {
      for (Observer& obs : observers_) {
        obs.OnPrinterRemoved(*p);
      }
    }
  } else {
    LOG(WARNING) << "Could not find printer" << printer_id;
  }

  return success;
}

void PrintersManager::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void PrintersManager::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

PrintersSyncBridge* PrintersManager::GetSyncBridge() {
  return sync_bridge_.get();
}

// This method is not thread safe and could interact poorly with readers of
// |recommended_printers_|.
void PrintersManager::UpdateRecommendedPrinters() {
  const PrefService* prefs = profile_->GetPrefs();

  const base::ListValue* values =
      prefs->GetList(prefs::kRecommendedNativePrinters);

  recommended_printer_ids_.clear();
  for (const auto& value : *values) {
    std::string printer_json;
    if (!value->GetAsString(&printer_json)) {
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
    printer_dictionary->SetString(printing::kPrinterId, id);

    recommended_printer_ids_.push_back(id);
    recommended_printers_[id] = std::move(printer_dictionary);
  }
}

}  // namespace chromeos
