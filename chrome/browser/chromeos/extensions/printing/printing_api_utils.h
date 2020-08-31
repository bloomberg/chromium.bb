// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_PRINTING_PRINTING_API_UTILS_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_PRINTING_PRINTING_API_UTILS_H_

#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "base/optional.h"
#include "chrome/browser/chromeos/printing/printer_error_codes.h"
#include "chrome/common/extensions/api/printing.h"

namespace base {
class Value;
}  // namespace base

namespace chromeos {
class Printer;
}  // namespace chromeos

namespace printing {
class PrintSettings;
struct PrinterSemanticCapsAndDefaults;
}  // namespace printing

namespace extensions {

struct DefaultPrinterRules {
  std::string kind;
  std::string id_pattern;
  std::string name_pattern;
};

// Parses the string containing
// |prefs::kPrintPreviewDefaultDestinationSelectionRules| value and returns
// default printer selection rules in the form declared above.
base::Optional<DefaultPrinterRules> GetDefaultPrinterRules(
    const std::string& default_destination_selection_rules);

api::printing::Printer PrinterToIdl(
    const chromeos::Printer& printer,
    const base::Optional<DefaultPrinterRules>& default_printer_rules,
    const base::flat_map<std::string, int>& recently_used_ranks);

api::printing::PrinterStatus PrinterStatusToIdl(
    chromeos::PrinterErrorCode status);

// Converts print ticket in CJT
// (https://developers.google.com/cloud-print/docs/cdd#cjt) format to
// printing::PrintSettings.
// Returns nullptr in case of invalid ticket.
std::unique_ptr<printing::PrintSettings> ParsePrintTicket(base::Value ticket);

// Checks if given print job settings are compatible with printer capabilities.
bool CheckSettingsAndCapabilitiesCompatibility(
    const printing::PrintSettings& settings,
    const printing::PrinterSemanticCapsAndDefaults& capabilities);

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_PRINTING_PRINTING_API_UTILS_H_
