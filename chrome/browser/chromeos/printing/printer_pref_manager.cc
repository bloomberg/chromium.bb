// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/printer_pref_manager.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/guid.h"
#include "base/json/json_reader.h"
#include "base/md5.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chromeos/printing/printer_configuration.h"
#include "chromeos/printing/printer_translator.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace chromeos {

namespace {

const base::ListValue* GetPrinterList(Profile* profile) {
  return profile->GetPrefs()->GetList(prefs::kPrintingDevices);
}

// Returns the printer with the matching |id| from the list |values|.  The
// returned value is mutable and |values| could be modified.
base::DictionaryValue* FindPrinterPref(const base::ListValue* values,
                                       const std::string& id) {
  for (const auto& value : *values) {
    base::DictionaryValue* printer_dictionary;
    if (!value->GetAsDictionary(&printer_dictionary))
      continue;

    std::string printer_id;
    if (printer_dictionary->GetString(printing::kPrinterId, &printer_id) &&
        id == printer_id)
      return printer_dictionary;
  }

  return nullptr;
}

void UpdatePrinterPref(
    Profile* profile,
    const std::string& id,
    std::unique_ptr<base::DictionaryValue> printer_dictionary) {
  ListPrefUpdate update(profile->GetPrefs(), prefs::kPrintingDevices);
  base::ListValue* printer_list = update.Get();
  DCHECK(printer_list) << "Register the printer preference";
  base::DictionaryValue* printer = FindPrinterPref(printer_list, id);
  if (!printer) {
    printer_list->Append(std::move(printer_dictionary));
    return;
  }

  printer->MergeDictionary(printer_dictionary.get());
}

}  // anonymous namespace

PrinterPrefManager::PrinterPrefManager(Profile* profile) : profile_(profile) {
  pref_change_registrar_.Init(profile->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kRecommendedNativePrinters,
      base::Bind(&PrinterPrefManager::UpdateRecommendedPrinters,
                 base::Unretained(this)));
  UpdateRecommendedPrinters();
}

PrinterPrefManager::~PrinterPrefManager() {}

// static
void PrinterPrefManager::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  // TODO(skau): Change to user_prefs::PrefRegistrySyncable::SYNCABLE_PREF) when
  // sync is implemented.
  registry->RegisterListPref(prefs::kPrintingDevices,
                             PrefRegistry::NO_REGISTRATION_FLAGS);
  registry->RegisterListPref(prefs::kRecommendedNativePrinters);
}

std::vector<std::unique_ptr<Printer>> PrinterPrefManager::GetPrinters() const {
  std::vector<std::unique_ptr<Printer>> printers;

  const base::ListValue* values = GetPrinterList(profile_);
  for (const auto& value : *values) {
    const base::DictionaryValue* printer_dictionary = nullptr;
    value->GetAsDictionary(&printer_dictionary);

    DCHECK(printer_dictionary);

    std::unique_ptr<Printer> printer =
        printing::PrefToPrinter(*printer_dictionary);
    if (printer)
      printers.push_back(std::move(printer));
  }

  return printers;
}

std::vector<std::unique_ptr<Printer>>
PrinterPrefManager::GetRecommendedPrinters() const {
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

std::unique_ptr<Printer> PrinterPrefManager::GetPrinter(
    const std::string& printer_id) const {
  // check for a policy printer first
  const auto& policy_printers = recommended_printers_;
  auto found = policy_printers.find(printer_id);
  if (found != policy_printers.end())
    return printing::RecommendedPrinterToPrinter(*(found->second));

  const base::ListValue* values = GetPrinterList(profile_);
  const base::DictionaryValue* printer = FindPrinterPref(values, printer_id);

  return printer ? printing::PrefToPrinter(*printer) : nullptr;
}

void PrinterPrefManager::RegisterPrinter(std::unique_ptr<Printer> printer) {
  if (printer->id().empty())
    printer->set_id(base::GenerateGUID());

  DCHECK_EQ(Printer::SRC_USER_PREFS, printer->source());
  std::unique_ptr<base::DictionaryValue> updated_printer =
      printing::PrinterToPref(*printer);
  UpdatePrinterPref(profile_, printer->id(), std::move(updated_printer));
}

bool PrinterPrefManager::RemovePrinter(const std::string& printer_id) {
  DCHECK(!printer_id.empty());
  ListPrefUpdate update(profile_->GetPrefs(), prefs::kPrintingDevices);
  base::ListValue* printer_list = update.Get();
  DCHECK(printer_list) << "Printer preference not registered";
  base::DictionaryValue* printer = FindPrinterPref(printer_list, printer_id);

  return printer && printer_list->Remove(*printer, nullptr);
}

// This method is not thread safe and could interact poorly with readers of
// |recommended_printers_|.
void PrinterPrefManager::UpdateRecommendedPrinters() {
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
