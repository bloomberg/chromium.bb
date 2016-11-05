// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_PRINTER_BACKEND_PROXY_H_
#define CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_PRINTER_BACKEND_PROXY_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/values.h"
#include "printing/backend/print_backend.h"

class Profile;

namespace printing {

using EnumeratePrintersCallback = base::Callback<void(const PrinterList&)>;

using PrinterSetupCallback =
    base::Callback<void(std::unique_ptr<base::DictionaryValue>)>;

// Returns the name of the default printer.
std::string GetDefaultPrinterOnBlockingPoolThread();

// Retrieves printers for display in the print dialog and calls |cb| with the
// list.  This method expects to be called on the UI thread.
void EnumeratePrinters(Profile* profile, const EnumeratePrintersCallback& cb);

// Verifies printer setup if needed then retrieves printer capabilities for
// |printer_name|.  |cb| is called with the capabilities dictionary or nullptr
// if one of the steps failed.
void ConfigurePrinterAndFetchCapabilities(Profile* profile,
                                          const std::string& printer_name,
                                          const PrinterSetupCallback& cb);

}  // namespace printing

#endif  // CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_PRINTER_BACKEND_PROXY_H_
