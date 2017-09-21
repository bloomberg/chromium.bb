// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/printer_capabilities.h"

#include <memory>
#include <string>
#include <utility>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "base/strings/string_piece.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/common/cloud_print/cloud_print_cdd_conversion.h"
#include "chrome/common/crash_keys.h"
#include "printing/backend/print_backend.h"
#include "printing/backend/print_backend_consts.h"

#if defined(OS_WIN)
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#endif

namespace printing {

const char kPrinterId[] = "printerId";
const char kPrinterCapabilities[] = "capabilities";

namespace {

// Returns a dictionary representing printer capabilities as CDD.  Returns
// an empty dictionary if a dictionary could not be generated.
std::unique_ptr<base::DictionaryValue>
GetPrinterCapabilitiesOnBlockingPoolThread(const std::string& device_name) {
  base::ThreadRestrictions::AssertIOAllowed();
  DCHECK(!device_name.empty());

  scoped_refptr<PrintBackend> print_backend(
      PrintBackend::CreateInstance(nullptr));

  VLOG(1) << "Get printer capabilities start for " << device_name;
  crash_keys::ScopedPrinterInfo crash_key(
      print_backend->GetPrinterDriverInfo(device_name));

  auto empty_capabilities = std::make_unique<base::DictionaryValue>();
  std::unique_ptr<base::DictionaryValue> printer_info;
  if (!print_backend->IsValidPrinter(device_name)) {
    LOG(WARNING) << "Invalid printer " << device_name;
    return empty_capabilities;
  }

  PrinterSemanticCapsAndDefaults info;
  if (!print_backend->GetPrinterSemanticCapsAndDefaults(device_name, &info)) {
    LOG(WARNING) << "Failed to get capabilities for " << device_name;
    return empty_capabilities;
  }

  return cloud_print::PrinterSemanticCapsAndDefaultsToCdd(info);
}

#if defined(OS_WIN)
std::string GetUserFriendlyName(const std::string& printer_name) {
  // |printer_name| may be a UNC path like \\printserver\printername.
  if (!base::StartsWith(printer_name, "\\\\",
                        base::CompareCase::INSENSITIVE_ASCII)) {
    return printer_name;
  }

  // If it is a UNC path, split the "printserver\printername" portion and
  // generate a friendly name, like Windows does.
  std::string printer_name_trimmed = printer_name.substr(2);
  std::vector<std::string> tokens = base::SplitString(
      printer_name_trimmed, "\\", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  if (tokens.size() != 2 || tokens[0].empty() || tokens[1].empty())
    return printer_name;
  return l10n_util::GetStringFUTF8(
      IDS_PRINT_PREVIEW_FRIENDLY_WIN_NETWORK_PRINTER_NAME,
      base::UTF8ToUTF16(tokens[1]), base::UTF8ToUTF16(tokens[0]));
}
#endif

}  // namespace

std::pair<std::string, std::string> GetPrinterNameAndDescription(
    const PrinterBasicInfo& printer) {
#if defined(OS_MACOSX) || defined(OS_CHROMEOS)
  // On Mac, |printer.printer_description| specifies the printer name and
  // |printer.printer_name| specifies the device name / printer queue name.
  // Chrome OS emulates the Mac behavior.
  const std::string& real_name = printer.printer_description;
  std::string real_description;
  const auto it = printer.options.find(kDriverNameTagName);
  if (it != printer.options.end())
    real_description = it->second;
  return std::make_pair(real_name, real_description);
#elif defined(OS_WIN)
  return std::make_pair(GetUserFriendlyName(printer.printer_name),
                        printer.printer_description);
#else
  return std::make_pair(printer.printer_name, printer.printer_description);
#endif
}

std::unique_ptr<base::DictionaryValue> GetSettingsOnBlockingPool(
    const std::string& device_name,
    const PrinterBasicInfo& basic_info) {
  base::ThreadRestrictions::AssertIOAllowed();

  const auto printer_name_description =
      GetPrinterNameAndDescription(basic_info);
  const std::string& printer_name = printer_name_description.first;
  const std::string& printer_description = printer_name_description.second;

  auto printer_info = base::MakeUnique<base::DictionaryValue>();
  printer_info->SetString(kPrinterId, device_name);
  printer_info->SetString(kSettingPrinterName, printer_name);
  printer_info->SetString(kSettingPrinterDescription, printer_description);
  printer_info->SetBoolean(
      kCUPSEnterprisePrinter,
      base::ContainsKey(basic_info.options, kCUPSEnterprisePrinter) &&
          basic_info.options.at(kCUPSEnterprisePrinter) == kValueTrue);

  printer_info->Set(kPrinterCapabilities,
                    GetPrinterCapabilitiesOnBlockingPoolThread(device_name));

  return printer_info;
}

}  // namespace printing
