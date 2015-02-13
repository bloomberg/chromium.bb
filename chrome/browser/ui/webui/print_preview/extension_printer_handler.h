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

namespace base {
class DictionaryValue;
class ListValue;
class RefCountedMemory;
}

namespace content {
class BrowserContext;
}

// Implementation of PrinterHandler interface backed by printerProvider
// extension API.
class ExtensionPrinterHandler : public PrinterHandler {
 public:
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
                  const std::string& ticket_json,
                  const scoped_refptr<base::RefCountedMemory>& print_data,
                  const PrinterHandler::PrintCallback& callback) override;

 private:
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

  base::WeakPtrFactory<ExtensionPrinterHandler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionPrinterHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_EXTENSION_PRINTER_HANDLER_H_
