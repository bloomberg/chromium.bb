// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PRINTING_SPECIFICS_TRANSLATION_H_
#define CHROME_BROWSER_CHROMEOS_PRINTING_SPECIFICS_TRANSLATION_H_

#include <memory>

#include "chromeos/printing/printer_configuration.h"
#include "components/sync/protocol/printer_specifics.pb.h"

namespace chromeos {
namespace printing {

std::unique_ptr<Printer> SpecificsToPrinter(
    const sync_pb::PrinterSpecifics& printer);

std::unique_ptr<sync_pb::PrinterSpecifics> PrinterToSpecifics(
    const Printer& printer);

// Merge fields from |printer| into |specifics|.
void MergePrinterToSpecifics(const Printer& printer,
                             sync_pb::PrinterSpecifics* specifics);

}  // namespace printing
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PRINTING_SPECIFICS_TRANSLATION_H_
