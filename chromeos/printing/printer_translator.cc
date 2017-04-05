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

// For historical reasons, the effective_make_and_model field is just
// effective_model for policy printers.
const char kEffectiveModel[] = "effective_model";

// printer fields
const char kDisplayName[] = "display_name";
const char kDescription[] = "description";
const char kManufacturer[] = "manufacturer";
const char kModel[] = "model";
const char kUri[] = "uri";
const char kUUID[] = "uuid";
const char kPpdResource[] = "ppd_resource";

// Converts |value| into a Printer object for the fields that are shared
// between pref printers and policy printers.
std::unique_ptr<Printer> DictionaryToPrinter(const DictionaryValue& value) {
  std::string id;
  if (!value.GetString(printing::kPrinterId, &id)) {
    LOG(WARNING) << "Record id required";
    return nullptr;
  }

  std::unique_ptr<Printer> printer = base::MakeUnique<Printer>(id);

  // Mandatory fields
  std::string display_name;
  if (value.GetString(kDisplayName, &display_name)) {
    printer->set_display_name(display_name);
  } else {
    LOG(WARNING) << "Display name required";
    return nullptr;
  }

  std::string uri;
  if (value.GetString(kUri, &uri)) {
    printer->set_uri(uri);
  } else {
    LOG(WARNING) << "Uri required";
    return nullptr;
  }

  // Optional fields
  std::string description;
  if (value.GetString(kDescription, &description))
    printer->set_description(description);

  std::string manufacturer;
  if (value.GetString(kManufacturer, &manufacturer))
    printer->set_manufacturer(manufacturer);

  std::string model;
  if (value.GetString(kModel, &model))
    printer->set_model(model);

  std::string uuid;
  if (value.GetString(kUUID, &uuid))
    printer->set_uuid(uuid);

  return printer;
}

}  // namespace

namespace printing {

const char kPrinterId[] = "id";

std::unique_ptr<Printer> RecommendedPrinterToPrinter(
    const base::DictionaryValue& pref) {
  std::unique_ptr<Printer> printer = DictionaryToPrinter(pref);
  if (!printer) {
    LOG(WARNING) << "Failed to parse policy printer.";
    return nullptr;
  }

  printer->set_source(Printer::SRC_POLICY);

  const DictionaryValue* ppd;
  std::string make_and_model;
  if (pref.GetDictionary(kPpdResource, &ppd) &&
      ppd->GetString(kEffectiveModel, &make_and_model)) {
    printer->mutable_ppd_reference()->effective_make_and_model = make_and_model;
  } else {
    // Make and model is mandatory
    LOG(WARNING) << "Missing model information for policy printer.";
    return nullptr;
  }

  return printer;
}

}  // namespace printing
}  // namespace chromeos
