// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_PRINTER_CAPABILITIES_H_
#define CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_PRINTER_CAPABILITIES_H_

#include <memory>
#include <string>
#include <utility>

#include "base/values.h"

namespace printing {

struct PrinterBasicInfo;

// Printer capability setting keys.
extern const char kPrinterId[];
extern const char kPrinterCapabilities[];

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

}  // namespace printing

#endif  // CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_PRINTER_CAPABILITIES_H_
