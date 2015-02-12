// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_PRINTER_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_PRINTER_HANDLER_H_

#include <string>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"

namespace base {
class DictionaryValue;
class ListValue;
class RefCountedMemory;
}

namespace content {
class BrowserContext;
}

// Wrapper around PrinterProviderAPI to be used by print preview.
// It makes request lifetime management easier, and hides details of more
// complex operations like printing from the print preview handler.
// TODO(tbarzic): Use the same interface for other printer types.
class PrinterHandler {
 public:
  using GetPrintersCallback =
      base::Callback<void(const base::ListValue& printers, bool done)>;
  using GetCapabilityCallback =
      base::Callback<void(const std::string& printer_id,
                          const base::DictionaryValue& capability)>;
  using PrintCallback =
      base::Callback<void(bool success, const std::string& error)>;

  // Creates an instance of an PrinterHandler for extension printers.
  static scoped_ptr<PrinterHandler> CreateForExtensionPrinters(
      content::BrowserContext* browser_context);

  virtual ~PrinterHandler() {}

  // Cancels all pending requests.
  virtual void Reset() = 0;

  // Starts getting available printers.
  // |callback| should be called in the response to the request.
  virtual void StartGetPrinters(const GetPrintersCallback& callback) = 0;

  // Starts getting printing capability of the printer with the provided
  // destination ID.
  // |callback| should be called in the response to the request.
  virtual void StartGetCapability(const std::string& destination_id,
                                  const GetCapabilityCallback& callback) = 0;

  // Starts a print request.
  // |print_job_settings|: The print job settings, including the printer ID and
  //     its capability.
  // |print_data|: The document bytes to print.
  // |callback| should be called in the response to the request.
  virtual void StartPrint(
      const base::DictionaryValue& print_job_settings,
      const scoped_refptr<base::RefCountedMemory>& print_data,
      const PrintCallback& callback) = 0;
};

#endif  // CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_PRINTER_HANDLER_H_
