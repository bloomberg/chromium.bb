// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/printing/printer_translator.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

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

// Populates the |printer| object with corresponding fields from |value|.
// Returns false if |value| is missing a required field.
bool DictionaryToPrinter(const DictionaryValue& value, Printer* printer) {
  // Mandatory fields
  std::string display_name;
  if (value.GetString(kDisplayName, &display_name)) {
    printer->set_display_name(display_name);
  } else {
    LOG(WARNING) << "Display name required";
    return false;
  }

  std::string uri;
  if (value.GetString(kUri, &uri)) {
    printer->set_uri(uri);
  } else {
    LOG(WARNING) << "Uri required";
    return false;
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

  std::string make_and_model = manufacturer;
  if (!manufacturer.empty() && !model.empty())
    make_and_model.append(" ");
  make_and_model.append(model);
  printer->set_make_and_model(make_and_model);

  std::string uuid;
  if (value.GetString(kUUID, &uuid))
    printer->set_uuid(uuid);

  return true;
}

}  // namespace

const char kPrinterId[] = "id";

std::unique_ptr<Printer> RecommendedPrinterToPrinter(
    const base::DictionaryValue& pref) {
  std::string id;
  if (!pref.GetString(kPrinterId, &id)) {
    LOG(WARNING) << "Record id required";
    return nullptr;
  }

  auto printer = std::make_unique<Printer>(id);
  if (!DictionaryToPrinter(pref, printer.get())) {
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

}  // namespace chromeos
