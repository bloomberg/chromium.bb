// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_LOCAL_PRINTER_HANDLER_DEFAULT_H_
#define CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_LOCAL_PRINTER_HANDLER_DEFAULT_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/print_preview/printer_handler.h"

class LocalPrinterHandlerDefault : public PrinterHandler {
 public:
  LocalPrinterHandlerDefault();
  ~LocalPrinterHandlerDefault() override;

  // PrinterHandler implementation.
  void Reset() override;
  void GetDefaultPrinter(DefaultPrinterCallback cb) override;
  void StartGetPrinters(const AddedPrintersCallback& added_printers_callback,
                        GetPrintersDoneCallback done_callback) override;
  void StartGetCapability(const std::string& destination_id,
                          GetCapabilityCallback callback) override;
  // Required by PrinterHandler interface but should never be called.
  void StartPrint(const std::string& destination_id,
                  const std::string& capability,
                  const base::string16& job_title,
                  const std::string& ticket_json,
                  const gfx::Size& page_size,
                  const scoped_refptr<base::RefCountedBytes>& print_data,
                  PrintCallback callback) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(LocalPrinterHandlerDefault);
};

#endif  // CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_LOCAL_PRINTER_HANDLER_DEFAULT_H_
