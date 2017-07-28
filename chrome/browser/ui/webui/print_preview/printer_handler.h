// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_PRINTER_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_PRINTER_HANDLER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string16.h"
#include "chrome/common/features.h"

namespace base {
class DictionaryValue;
class ListValue;
class RefCountedBytes;
class Value;
}

class Profile;

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
      base::Callback<void(const base::DictionaryValue& capability)>;
  using PrintCallback =
      base::Callback<void(bool success, const base::Value& error)>;
  using GetPrinterInfoCallback =
      base::Callback<void(const base::DictionaryValue& printer_info)>;

  // Creates an instance of a PrinterHandler for extension printers.
  static std::unique_ptr<PrinterHandler> CreateForExtensionPrinters(
      Profile* profile);

#if BUILDFLAG(ENABLE_SERVICE_DISCOVERY)
  // Creates an instance of a PrinterHandler for privet printers.
  static std::unique_ptr<PrinterHandler> CreateForPrivetPrinters(
      Profile* profile);
#endif

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

  // Starts granting access to the given provisional printer. The print handler
  // will respond with more information about the printer including its non-
  // provisional printer id.
  // |callback| should be called in response to the request.
  virtual void StartGrantPrinterAccess(
      const std::string& printer_id,
      const GetPrinterInfoCallback& callback) = 0;

  // Starts a print request.
  // |destination_id|: The printer to which print job should be sent.
  // |capability|: Capability reported by the printer.
  // |job_title|: The  title used for print job.
  // |ticket_json|: The print job ticket as JSON string.
  // |page_size|: The document page size.
  // |print_data|: The document bytes to print.
  // |callback| should be called in the response to the request.
  // TODO(tbarzic): Page size should be extracted from print data.
  virtual void StartPrint(
      const std::string& destination_id,
      const std::string& capability,
      const base::string16& job_title,
      const std::string& ticket_json,
      const gfx::Size& page_size,
      const scoped_refptr<base::RefCountedBytes>& print_data,
      const PrintCallback& callback) = 0;
};

#endif  // CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_PRINTER_HANDLER_H_
