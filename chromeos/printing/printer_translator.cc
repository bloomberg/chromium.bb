// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/printing/printer_translator.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "chromeos/printing/printer_configuration.h"

using base::DictionaryValue;

namespace chromeos {

namespace {

// PPD reference fields
const char kUserSuppliedPpdUrl[] = "user_supplied_ppd_url";
// TODO(justincarlson) -- This should be changed to effective_make_and_model to
// match the implementation.
const char kEffectiveModel[] = "effective_model";

// printer fields
const char kDisplayName[] = "display_name";
const char kDescription[] = "description";
const char kManufacturer[] = "manufacturer";
const char kModel[] = "model";
const char kUri[] = "uri";
const char kPpdReference[] = "ppd_reference";
const char kUUID[] = "uuid";

// The name of the PpdResource for policy printers.
const char kPpdResource[] = "ppd_resource";

Printer::PpdReference DictionaryToPpdReference(
    const base::DictionaryValue* value) {
  Printer::PpdReference ppd;
  value->GetString(kUserSuppliedPpdUrl, &ppd.user_supplied_ppd_url);
  value->GetString(kEffectiveModel, &ppd.effective_make_and_model);
  return ppd;
}

// Convert a PpdReference struct to a DictionaryValue.
std::unique_ptr<base::DictionaryValue> PpdReferenceToDictionary(
    const Printer::PpdReference& ppd) {
  auto dictionary = base::MakeUnique<DictionaryValue>();
  if (!ppd.user_supplied_ppd_url.empty()) {
    dictionary->SetString(kUserSuppliedPpdUrl, ppd.user_supplied_ppd_url);
  }
  if (!ppd.effective_make_and_model.empty()) {
    dictionary->SetString(kEffectiveModel, ppd.effective_make_and_model);
  }
  return dictionary;
}

// Converts |value| into a Printer object for the fields that are shared
// between pref printers and policy printers.
std::unique_ptr<Printer> DictionaryToPrinter(const DictionaryValue& value) {
  std::string id;
  if (!value.GetString(printing::kPrinterId, &id)) {
    LOG(WARNING) << "Record id required";
    return nullptr;
  }

  std::unique_ptr<Printer> printer = base::MakeUnique<Printer>(id);

  std::string display_name;
  if (value.GetString(kDisplayName, &display_name))
    printer->set_display_name(display_name);

  std::string description;
  if (value.GetString(kDescription, &description))
    printer->set_description(description);

  std::string manufacturer;
  if (value.GetString(kManufacturer, &manufacturer))
    printer->set_manufacturer(manufacturer);

  std::string model;
  if (value.GetString(kModel, &model))
    printer->set_model(model);

  std::string uri;
  if (value.GetString(kUri, &uri))
    printer->set_uri(uri);

  std::string uuid;
  if (value.GetString(kUUID, &uuid))
    printer->set_uuid(uuid);

  return printer;
}

}  // namespace

namespace printing {

const char kPrinterId[] = "id";

std::unique_ptr<Printer> PrefToPrinter(const DictionaryValue& value) {
  if (!value.HasKey(kPrinterId)) {
    LOG(WARNING) << "Record id required";
    return nullptr;
  }

  std::unique_ptr<Printer> printer = DictionaryToPrinter(value);
  printer->set_source(Printer::SRC_USER_PREFS);

  const DictionaryValue* ppd;
  if (value.GetDictionary(kPpdReference, &ppd)) {
    *printer->mutable_ppd_reference() = DictionaryToPpdReference(ppd);
  }

  return printer;
}

std::unique_ptr<base::DictionaryValue> PrinterToPref(const Printer& printer) {
  std::unique_ptr<DictionaryValue> dictionary =
      base::MakeUnique<base::DictionaryValue>();
  dictionary->SetString(kPrinterId, printer.id());
  dictionary->SetString(kDisplayName, printer.display_name());
  dictionary->SetString(kDescription, printer.description());
  dictionary->SetString(kManufacturer, printer.manufacturer());
  dictionary->SetString(kModel, printer.model());
  dictionary->SetString(kUri, printer.uri());
  dictionary->SetString(kUUID, printer.uuid());

  dictionary->Set(kPpdReference,
                  PpdReferenceToDictionary(printer.ppd_reference()));

  return dictionary;
}

std::unique_ptr<Printer> RecommendedPrinterToPrinter(
    const base::DictionaryValue& pref) {
  std::unique_ptr<Printer> printer = DictionaryToPrinter(pref);
  printer->set_source(Printer::SRC_POLICY);

  const DictionaryValue* ppd;
  if (pref.GetDictionary(kPpdResource, &ppd)) {
    *printer->mutable_ppd_reference() = DictionaryToPpdReference(ppd);
  }

  return printer;
}

}  // namespace printing
}  // namespace chromeos
