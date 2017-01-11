// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_EXTENSION_PRINTER_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_EXTENSION_PRINTER_HANDLER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "chrome/browser/ui/webui/print_preview/printer_handler.h"
#include "extensions/browser/api/printer_provider/printer_provider_api.h"

namespace base {
class DictionaryValue;
class ListValue;
class RefCountedMemory;
}

namespace content {
class BrowserContext;
}

namespace cloud_devices {
class CloudDeviceDescription;
}

namespace device {
class UsbDevice;
}

namespace gfx {
class Size;
}

namespace printing {
class PWGRasterConverter;
}

// Implementation of PrinterHandler interface backed by printerProvider
// extension API.
class ExtensionPrinterHandler : public PrinterHandler {
 public:
  using PrintJobCallback = base::Callback<void(
      std::unique_ptr<extensions::PrinterProviderPrintJob>)>;

  explicit ExtensionPrinterHandler(content::BrowserContext* browser_context);

  ~ExtensionPrinterHandler() override;

  // PrinterHandler implementation:
  void Reset() override;
  void StartGetPrinters(
      const PrinterHandler::GetPrintersCallback& callback) override;
  void StartGetCapability(
      const std::string& destination_id,
      const PrinterHandler::GetCapabilityCallback& calback) override;
  // TODO(tbarzic): It might make sense to have the strings in a single struct.
  void StartPrint(const std::string& destination_id,
                  const std::string& capability,
                  const base::string16& job_title,
                  const std::string& ticket_json,
                  const gfx::Size& page_size,
                  const scoped_refptr<base::RefCountedMemory>& print_data,
                  const PrinterHandler::PrintCallback& callback) override;
  void StartGrantPrinterAccess(
      const std::string& printer_id,
      const PrinterHandler::GetPrinterInfoCallback& callback) override;

 private:
  friend class ExtensionPrinterHandlerTest;

  void SetPWGRasterConverterForTesting(
      std::unique_ptr<printing::PWGRasterConverter> pwg_raster_converter);

  // Converts |data| to PWG raster format (from PDF) for a printer described
  // by |printer_description|.
  // |callback| is called with the converted data.
  void ConvertToPWGRaster(
      const scoped_refptr<base::RefCountedMemory>& data,
      const cloud_devices::CloudDeviceDescription& printer_description,
      const cloud_devices::CloudDeviceDescription& ticket,
      const gfx::Size& page_size,
      std::unique_ptr<extensions::PrinterProviderPrintJob> job,
      const PrintJobCallback& callback);

  // Sets print job document data and dispatches it using printerProvider API.
  void DispatchPrintJob(
      const PrinterHandler::PrintCallback& callback,
      std::unique_ptr<extensions::PrinterProviderPrintJob> print_job);

  // Methods used as wrappers to callbacks for extensions::PrinterProviderAPI
  // methods, primarily so the callbacks can be bound to this class' weak ptr.
  // They just propagate results to callbacks passed to them.
  void WrapGetPrintersCallback(
      const PrinterHandler::GetPrintersCallback& callback,
      const base::ListValue& printers,
      bool done);
  void WrapGetCapabilityCallback(
      const PrinterHandler::GetCapabilityCallback& callback,
      const std::string& destination_id,
      const base::DictionaryValue& capability);
  void WrapPrintCallback(const PrinterHandler::PrintCallback& callback,
                         bool success,
                         const std::string& status);
  void WrapGetPrinterInfoCallback(const GetPrinterInfoCallback& callback,
                                  const base::DictionaryValue& printer_info);

  void OnUsbDevicesEnumerated(
      const PrinterHandler::GetPrintersCallback& callback,
      const std::vector<scoped_refptr<device::UsbDevice>>& devices);

  content::BrowserContext* browser_context_;

  std::unique_ptr<printing::PWGRasterConverter> pwg_raster_converter_;
  int pending_enumeration_count_ = 0;

  base::WeakPtrFactory<ExtensionPrinterHandler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionPrinterHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_EXTENSION_PRINTER_HANDLER_H_
