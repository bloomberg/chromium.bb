// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/chromeos/printing/specifics_translation.h"
#include "chromeos/printing/printer_configuration.h"
#include "components/sync/protocol/printer_specifics.pb.h"

namespace chromeos {
namespace printing {

namespace {

Printer::PpdReference SpecificsToPpd(
    const sync_pb::PrinterPPDReference& specifics) {
  Printer::PpdReference ref;
  if (specifics.has_user_supplied_ppd_url()) {
    ref.user_supplied_ppd_url = specifics.user_supplied_ppd_url();
  }

  if (specifics.has_effective_model()) {
    ref.effective_model = specifics.effective_model();

    if (specifics.has_effective_manufacturer())
      ref.effective_manufacturer = specifics.effective_manufacturer();
  }

  return ref;
}

sync_pb::PrinterPPDReference ReferenceToSpecifics(
    const Printer::PpdReference& ref) {
  sync_pb::PrinterPPDReference specifics;
  if (!ref.user_supplied_ppd_url.empty()) {
    specifics.set_user_supplied_ppd_url(ref.user_supplied_ppd_url);
  }

  if (!ref.effective_model.empty()) {
    specifics.set_effective_model(ref.effective_model);

    if (!ref.effective_manufacturer.empty())
      specifics.set_effective_manufacturer(ref.effective_manufacturer);
  }

  return specifics;
}

}  // namespace

std::unique_ptr<Printer> SpecificsToPrinter(
    const sync_pb::PrinterSpecifics& specifics) {
  DCHECK(!specifics.id().empty());

  auto printer = base::MakeUnique<Printer>(specifics.id());
  printer->set_display_name(specifics.display_name());
  printer->set_description(specifics.description());
  printer->set_manufacturer(specifics.manufacturer());
  printer->set_model(specifics.model());
  printer->set_uri(specifics.uri());
  printer->set_uuid(specifics.uuid());

  *printer->mutable_ppd_reference() = SpecificsToPpd(specifics.ppd_reference());

  return printer;
}

std::unique_ptr<sync_pb::PrinterSpecifics> PrinterToSpecifics(
    const Printer& printer) {
  DCHECK(!printer.id().empty());

  auto specifics = base::MakeUnique<sync_pb::PrinterSpecifics>();
  specifics->set_id(printer.id());

  if (!printer.display_name().empty())
    specifics->set_display_name(printer.display_name());

  if (!printer.description().empty())
    specifics->set_description(printer.description());

  if (!printer.manufacturer().empty())
    specifics->set_manufacturer(printer.manufacturer());

  if (!printer.model().empty())
    specifics->set_model(printer.model());

  if (!printer.uri().empty())
    specifics->set_uri(printer.uri());

  if (!printer.uuid().empty())
    specifics->set_uuid(printer.uuid());

  *specifics->mutable_ppd_reference() =
      ReferenceToSpecifics(printer.ppd_reference());

  return specifics;
}

}  // namespace printing
}  // namespace chromeos
