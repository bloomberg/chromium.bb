// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_PRINTER_CAPABILITIES_H_
#define CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_PRINTER_CAPABILITIES_H_

#include <memory>
#include <string>
#include <utility>

#include "base/values.h"
#include "chrome/browser/ui/webui/print_preview/printer_handler.h"
#include "printing/backend/print_backend.h"

namespace printing {

struct PrinterBasicInfo;

// Printer capability setting keys.
extern const char kOptionKey[];
extern const char kPrinter[];
extern const char kTypeKey[];
extern const char kSelectCapKey[];
extern const char kSelectString[];
extern const char kTypeKey[];
extern const char kVendorCapabilityKey[];

// Extracts the printer display name and description from the
// appropriate fields in |printer| for the platform.
std::pair<std::string, std::string> GetPrinterNameAndDescription(
    const PrinterBasicInfo& printer);

// Returns the JSON representing printer capabilities for the device registered
// as |device_name| in the PrinterBackend.  The returned dictionary is suitable
// for passage to the WebUI.
std::unique_ptr<base::DictionaryValue> GetSettingsOnBlockingPool(
    const std::string& device_name,
    const PrinterBasicInfo& basic_info);

// Converts |printer_list| to a base::ListValue form, runs |callback| with the
// converted list as the argument if it is not empty, and runs |done_callback|.
void ConvertPrinterListForCallback(
    const PrinterHandler::AddedPrintersCallback& callback,
    PrinterHandler::GetPrintersDoneCallback done_callback,
    const printing::PrinterList& printer_list);

// Returns a unique_ptr to a sanitized version of |cdd| to prevent possible JS
// errors in Print Preview. Will remove null items from lists or options lists
// and remove any lists/options that are empty or only contain null values.
std::unique_ptr<base::DictionaryValue> ValidateCddForPrintPreview(
    const base::DictionaryValue& cdd);
}  // namespace printing

#endif  // CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_PRINTER_CAPABILITIES_H_
