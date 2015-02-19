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

namespace gfx {
class Size;
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
  // |destination_id|: The printer to which print job should be sent.
  // |capability|: Capability reported by the printer.
  // |ticket_json|: The print job ticket as JSON string.
  // |page_size|: The document page size.
  // |print_data|: The document bytes to print.
  // |callback| should be called in the response to the request.
  // TODO(tbarzic): Page size should be extracted from print data.
  virtual void StartPrint(
      const std::string& destination_id,
      const std::string& capability,
      const std::string& ticket_json,
      const gfx::Size& page_size,
      const scoped_refptr<base::RefCountedMemory>& print_data,
      const PrintCallback& callback) = 0;
};

#endif  // CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_PRINTER_HANDLER_H_
