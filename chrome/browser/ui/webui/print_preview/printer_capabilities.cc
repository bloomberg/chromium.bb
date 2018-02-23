// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/printer_capabilities.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "base/stl_util.h"
#include "base/strings/string_piece.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/printing/print_preview_dialog_controller.h"
#include "chrome/browser/printing/print_view_manager.h"
#include "chrome/browser/ui/webui/print_preview/print_preview_ui.h"
#include "chrome/browser/ui/webui/print_preview/printer_handler.h"
#include "components/crash/core/common/crash_keys.h"
#include "components/printing/common/cloud_print_cdd_conversion.h"
#include "content/public/browser/render_frame_host.h"
#include "printing/backend/print_backend.h"
#include "printing/backend/print_backend_consts.h"
#include "printing/page_range.h"

#if defined(OS_WIN)
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#endif

namespace printing {

const char kPrinter[] = "printer";

// Keys for a dictionary specifying a custom vendor capability. See
// settings/advanced_settings/advanced_settings_item.js in
// chrome/browser/resources/print_preview.
const char kOptionKey[] = "option";
const char kSelectCapKey[] = "select_cap";
const char kSelectString[] = "SELECT";
const char kTypeKey[] = "type";

// The dictionary key for the CDD item containing custom vendor capabilities.
const char kVendorCapabilityKey[] = "vendor_capability";

namespace {

// Returns a dictionary representing printer capabilities as CDD.  Returns
// an empty dictionary if a dictionary could not be generated.
std::unique_ptr<base::DictionaryValue>
GetPrinterCapabilitiesOnBlockingPoolThread(const std::string& device_name) {
  base::AssertBlockingAllowed();
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

void PrintersToValues(const PrinterList& printer_list,
                      base::ListValue* printers) {
  for (const PrinterBasicInfo& printer : printer_list) {
    auto printer_info = std::make_unique<base::DictionaryValue>();
    printer_info->SetString(kSettingDeviceName, printer.printer_name);

    const auto printer_name_description = GetPrinterNameAndDescription(printer);
    const std::string& printer_name = printer_name_description.first;
    const std::string& printer_description = printer_name_description.second;
    printer_info->SetString(kSettingPrinterName, printer_name);
    printer_info->SetString(kSettingPrinterDescription, printer_description);

    auto options = std::make_unique<base::DictionaryValue>();
    for (const auto opt_it : printer.options)
      options->SetString(opt_it.first, opt_it.second);

    printer_info->SetBoolean(
        kCUPSEnterprisePrinter,
        base::ContainsKey(printer.options, kCUPSEnterprisePrinter) &&
            printer.options.at(kCUPSEnterprisePrinter) == kValueTrue);

    printer_info->Set(kSettingPrinterOptions, std::move(options));

    printers->Append(std::move(printer_info));

    VLOG(1) << "Found printer " << printer_name << " with device name "
            << printer.printer_name;
  }
}

template <typename Predicate>
base::Value GetFilteredList(const base::Value* list, Predicate pred) {
  auto out_list = list->Clone();
  base::EraseIf(out_list.GetList(), pred);
  return out_list;
}

bool ValueIsNull(const base::Value& val) {
  return val.is_none();
}

bool VendorCapabilityInvalid(const base::Value& val) {
  if (!val.is_dict())
    return true;
  const base::Value* option_type =
      val.FindKeyOfType(kTypeKey, base::Value::Type::STRING);
  if (!option_type)
    return true;
  if (option_type->GetString() != kSelectString)
    return false;
  const base::Value* select_cap =
      val.FindKeyOfType(kSelectCapKey, base::Value::Type::DICTIONARY);
  if (!select_cap)
    return true;
  const base::Value* options_list =
      select_cap->FindKeyOfType(kOptionKey, base::Value::Type::LIST);
  if (!options_list || options_list->GetList().empty() ||
      GetFilteredList(options_list, ValueIsNull).GetList().empty()) {
    return true;
  }
  return false;
}

void SystemDialogDone(const base::Value& error) {
  // intentional no-op
}

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
  base::AssertBlockingAllowed();

  const auto printer_name_description =
      GetPrinterNameAndDescription(basic_info);
  const std::string& printer_name = printer_name_description.first;
  const std::string& printer_description = printer_name_description.second;

  auto printer_info = std::make_unique<base::DictionaryValue>();
  printer_info->SetString(kSettingDeviceName, device_name);
  printer_info->SetString(kSettingPrinterName, printer_name);
  printer_info->SetString(kSettingPrinterDescription, printer_description);
  printer_info->SetBoolean(
      kCUPSEnterprisePrinter,
      base::ContainsKey(basic_info.options, kCUPSEnterprisePrinter) &&
          basic_info.options.at(kCUPSEnterprisePrinter) == kValueTrue);

  auto printer_info_capabilities = std::make_unique<base::DictionaryValue>();
  printer_info_capabilities->SetDictionary(kPrinter, std::move(printer_info));
  printer_info_capabilities->Set(
      kSettingCapabilities,
      GetPrinterCapabilitiesOnBlockingPoolThread(device_name));

  return printer_info_capabilities;
}

void ConvertPrinterListForCallback(
    const PrinterHandler::AddedPrintersCallback& callback,
    PrinterHandler::GetPrintersDoneCallback done_callback,
    const PrinterList& printer_list) {
  base::ListValue printers;
  PrintersToValues(printer_list, &printers);

  VLOG(1) << "Enumerate printers finished, found " << printers.GetSize()
          << " printers";
  if (!printers.empty())
    callback.Run(printers);
  std::move(done_callback).Run();
}

std::unique_ptr<base::DictionaryValue> ValidateCddForPrintPreview(
    const base::DictionaryValue& cdd) {
  auto validated_cdd =
      base::DictionaryValue::From(base::Value::ToUniquePtrValue(cdd.Clone()));
  const base::Value* caps = cdd.FindKey(kPrinter);
  if (!caps || !caps->is_dict())
    return validated_cdd;
  validated_cdd->RemoveKey(kPrinter);
  auto out_caps = std::make_unique<base::DictionaryValue>();
  for (const auto& capability : caps->DictItems()) {
    const auto& key = capability.first;
    const base::Value& value = capability.second;

    const base::Value* list = nullptr;
    if (value.is_dict())
      list = value.FindKeyOfType(kOptionKey, base::Value::Type::LIST);
    else if (value.is_list())
      list = &value;
    if (!list) {
      out_caps->SetKey(key, value.Clone());
      continue;
    }

    bool is_vendor_capability = key == kVendorCapabilityKey;
    base::Value out_list = GetFilteredList(
        list, is_vendor_capability ? VendorCapabilityInvalid : ValueIsNull);
    if (out_list.GetList().empty())  // leave out empty lists.
      continue;
    if (is_vendor_capability) {
      // Need to also filter the individual capability lists.
      for (auto& vendor_option : out_list.GetList()) {
        const base::Value* option_type =
            vendor_option.FindKeyOfType(kTypeKey, base::Value::Type::STRING);
        if (option_type->GetString() != kSelectString)
          continue;

        base::Value* options_dict = vendor_option.FindKeyOfType(
            kSelectCapKey, base::Value::Type::DICTIONARY);
        const base::Value* options_list =
            options_dict->FindKeyOfType(kOptionKey, base::Value::Type::LIST);
        options_dict->SetKey(kOptionKey,
                             GetFilteredList(options_list, ValueIsNull));
      }
    }
    if (value.is_dict()) {
      base::Value option_dict(base::Value::Type::DICTIONARY);
      option_dict.SetKey(kOptionKey, std::move(out_list));
      out_caps->SetKey(key, std::move(option_dict));
    } else {
      out_caps->SetKey(key, std::move(out_list));
    }
  }
  validated_cdd->SetDictionary(kPrinter, std::move(out_caps));
  return validated_cdd;
}

void StartLocalPrint(const std::string& ticket_json,
                     const scoped_refptr<base::RefCountedBytes>& print_data,
                     content::WebContents* preview_web_contents,
                     PrinterHandler::PrintCallback callback) {
  std::unique_ptr<base::DictionaryValue> job_settings =
      base::DictionaryValue::From(base::JSONReader::Read(ticket_json));
  if (!job_settings) {
    std::move(callback).Run(base::Value("Invalid settings"));
    return;
  }

  // Get print view manager.
  PrintPreviewDialogController* dialog_controller =
      PrintPreviewDialogController::GetInstance();
  content::WebContents* initiator =
      dialog_controller ? dialog_controller->GetInitiator(preview_web_contents)
                        : nullptr;
  PrintViewManager* print_view_manager =
      initiator ? PrintViewManager::FromWebContents(initiator) : nullptr;
  if (!print_view_manager) {
    std::move(callback).Run(base::Value("Initiator closed"));
    return;
  }

  bool system_dialog = false;
  job_settings->GetBoolean(printing::kSettingShowSystemDialog, &system_dialog);
  bool open_in_pdf = false;
  job_settings->GetBoolean(printing::kSettingOpenPDFInPreview, &open_in_pdf);
  if (system_dialog || open_in_pdf) {
    // Run the callback early, or the modal dialogs will prevent the preview
    // from closing until they do.
    std::move(callback).Run(base::Value());
    callback = base::BindOnce(&SystemDialogDone);
  }
  print_view_manager->PrintForPrintPreview(std::move(job_settings), print_data,
                                           preview_web_contents->GetMainFrame(),
                                           std::move(callback));
}
}  // namespace printing
