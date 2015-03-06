// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_EXTENSION_PRINTER_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_EXTENSION_PRINTER_HANDLER_H_

#include <string>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/print_preview/printer_handler.h"
#include "extensions/browser/api/printer_provider/printer_provider_api.h"

namespace base {
class DictionaryValue;
class ListValue;
class RefCountedMemory;
class TaskRunner;
}

namespace content {
class BrowserContext;
}

namespace cloud_devices {
class CloudDeviceDescription;
}

namespace gfx {
class Size;
}

namespace local_discovery {
class PWGRasterConverter;
}

// Implementation of PrinterHandler interface backed by printerProvider
// extension API.
class ExtensionPrinterHandler : public PrinterHandler {
 public:
  using PrintJobCallback =
      base::Callback<void(scoped_ptr<extensions::PrinterProviderPrintJob>)>;

  ExtensionPrinterHandler(
      content::BrowserContext* browser_context,
      const scoped_refptr<base::TaskRunner>& slow_task_runner);

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
                  const std::string& ticket_json,
                  const gfx::Size& page_size,
                  const scoped_refptr<base::RefCountedMemory>& print_data,
                  const PrinterHandler::PrintCallback& callback) override;

 private:
  friend class ExtensionPrinterHandlerTest;

  void SetPwgRasterConverterForTesting(
      scoped_ptr<local_discovery::PWGRasterConverter> pwg_raster_converter);

  // Converts |data| to PWG raster format (from PDF) for a printer described
  // by |printer_description|.
  // |callback| is called with the converted data.
  void ConvertToPWGRaster(
      const scoped_refptr<base::RefCountedMemory>& data,
      const cloud_devices::CloudDeviceDescription& printer_description,
      const cloud_devices::CloudDeviceDescription& ticket,
      const gfx::Size& page_size,
      scoped_ptr<extensions::PrinterProviderPrintJob> job,
      const PrintJobCallback& callback);

  // Sets print job document data and dispatches it using printerProvider API.
  void DispatchPrintJob(
      const PrinterHandler::PrintCallback& callback,
      scoped_ptr<extensions::PrinterProviderPrintJob> print_job);

  // Methods used as wrappers to callbacks for extensions::PrinterProviderAPI
  // methods, primarily so the callbacks can be bound to this class' weak ptr.
  // They just propagate results to callbacks passed to them.
  void WrapGetPrintersCallback(
      const PrinterHandler::GetPrintersCallback& callback,
      const base::ListValue& pritners,
      bool done);
  void WrapGetCapabilityCallback(
      const PrinterHandler::GetCapabilityCallback& callback,
      const std::string& destination_id,
      const base::DictionaryValue& capability);
  void WrapPrintCallback(const PrinterHandler::PrintCallback& callback,
                         bool success,
                         const std::string& status);

  content::BrowserContext* browser_context_;

  scoped_ptr<local_discovery::PWGRasterConverter> pwg_raster_converter_;

  scoped_refptr<base::TaskRunner> slow_task_runner_;

  base::WeakPtrFactory<ExtensionPrinterHandler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionPrinterHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_EXTENSION_PRINTER_HANDLER_H_
