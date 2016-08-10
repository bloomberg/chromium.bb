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

// ppd fields
const char kPPDid[] = "id";
const char kFileName[] = "file_name";
const char kVersionNumber[] = "version_number";
const char kFromQuirks[] = "from_quirks_server";

// printer fields
const char kDisplayName[] = "display_name";
const char kDescription[] = "description";
const char kManufacturer[] = "manufacturer";
const char kModel[] = "model";
const char kUri[] = "uri";
const char kPPD[] = "ppd";
const char kUUID[] = "uuid";

std::unique_ptr<Printer::PPDFile> DictionaryToPPDFile(
    const base::DictionaryValue* value) {
  std::unique_ptr<Printer::PPDFile> ppd = base::MakeUnique<Printer::PPDFile>();
  // struct PPDFile defines default values for these fields so there is no need
  // to check return values.
  value->GetInteger(kPPDid, &ppd->id);
  value->GetString(kFileName, &ppd->file_name);
  value->GetInteger(kVersionNumber, &ppd->version_number);
  value->GetBoolean(kFromQuirks, &ppd->from_quirks_server);
  return ppd;
}

std::unique_ptr<base::DictionaryValue> PPDFileToDictionary(
    const Printer::PPDFile& ppd) {
  std::unique_ptr<base::DictionaryValue> dictionary =
      base::MakeUnique<base::DictionaryValue>();
  dictionary->SetInteger(kPPDid, ppd.id);
  dictionary->SetString(kFileName, ppd.file_name);
  dictionary->SetInteger(kVersionNumber, ppd.version_number);
  dictionary->SetBoolean(kFromQuirks, ppd.from_quirks_server);
  return dictionary;
}

}  // namespace

namespace printing {

const char kPrinterId[] = "id";

std::unique_ptr<Printer> PrefToPrinter(const DictionaryValue& value) {
  std::string id;
  if (!value.GetString(kPrinterId, &id)) {
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

  const DictionaryValue* ppd;
  if (value.GetDictionary(kPPD, &ppd)) {
    printer->SetPPD(DictionaryToPPDFile(ppd));
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

  dictionary->Set(kPPD, PPDFileToDictionary(printer.ppd()));

  return dictionary;
}

}  // namespace printing

}  // namespace chromeos
