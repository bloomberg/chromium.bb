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

namespace content {
class WebContents;
}

namespace gfx {
class Size;
}

namespace printing {
class StickySettings;
}

class Profile;

// Wrapper around PrinterProviderAPI to be used by print preview.
// It makes request lifetime management easier, and hides details of more
// complex operations like printing from the print preview handler. This class
// expects to be created and all functions called on the UI thread.
class PrinterHandler {
 public:
  using DefaultPrinterCallback =
      base::OnceCallback<void(const std::string& printer_name)>;
  using AddedPrintersCallback =
      base::RepeatingCallback<void(const base::ListValue& printers)>;
  using GetPrintersDoneCallback = base::OnceClosure;
  // |capability| should contain a CDD with key printing::kSettingCapabilities.
  // It may also contain other information about the printer in a dictionary
  // with key printing::kPrinter.
  // If |capability| is null, empty, or does not contain a dictionary with key
  // printing::kSettingCapabilities, this indicates a failure to retrieve
  // capabilities.
  // If the dictionary with key printing::kSettingCapabilities is
  // empty, this indicates capabilities were retrieved but the printer does
  // not support any of the capability fields in a CDD.
  using GetCapabilityCallback = base::OnceCallback<void(
      std::unique_ptr<base::DictionaryValue> capability)>;
  using PrintCallback = base::OnceCallback<void(const base::Value& error)>;
  using GetPrinterInfoCallback =
      base::OnceCallback<void(const base::DictionaryValue& printer_info)>;

  // Creates an instance of a PrinterHandler for extension printers.
  static std::unique_ptr<PrinterHandler> CreateForExtensionPrinters(
      Profile* profile);

  // Creates an instance of a PrinterHandler for PDF printer.
  static std::unique_ptr<PrinterHandler> CreateForPdfPrinter(
      Profile* profile,
      content::WebContents* preview_web_contents,
      printing::StickySettings* sticky_settings);

  static std::unique_ptr<PrinterHandler> CreateForLocalPrinters(
      content::WebContents* preview_web_contents,
      Profile* profile);

#if BUILDFLAG(ENABLE_SERVICE_DISCOVERY)
  // Creates an instance of a PrinterHandler for privet printers.
  static std::unique_ptr<PrinterHandler> CreateForPrivetPrinters(
      Profile* profile);
#endif

  virtual ~PrinterHandler() {}

  // Cancels all pending requests.
  virtual void Reset() = 0;

  // Returns the name of the default printer through |cb|. Must be overridden
  // by implementations that expect this method to be called.
  virtual void GetDefaultPrinter(DefaultPrinterCallback cb);

  // Starts getting available printers.
  // |added_printers_callback| should be called in the response when printers
  // are found. May be called multiple times, or never if there are no printers
  // to add.
  // |done_callback| must be called exactly once when the search is complete.
  virtual void StartGetPrinters(
      const AddedPrintersCallback& added_printers_callback,
      GetPrintersDoneCallback done_callback) = 0;

  // Starts getting printing capability of the printer with the provided
  // destination ID.
  // |callback| should be called in the response to the request.
  virtual void StartGetCapability(const std::string& destination_id,
                                  GetCapabilityCallback callback) = 0;

  // Starts granting access to the given provisional printer. The print handler
  // will respond with more information about the printer including its non-
  // provisional printer id. Must be overridden by implementations that expect
  // this method to be called.
  // |callback| should be called in response to the request.
  virtual void StartGrantPrinterAccess(const std::string& printer_id,
                                       GetPrinterInfoCallback callback);

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
      PrintCallback callback) = 0;
};

#endif  // CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_PRINTER_HANDLER_H_
