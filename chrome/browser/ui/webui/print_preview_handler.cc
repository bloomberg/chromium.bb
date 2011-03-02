// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview_handler.h"

#include "base/values.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "printing/backend/print_backend.h"

PrintPreviewHandler::PrintPreviewHandler()
    : print_backend_(printing::PrintBackend::CreateInstance(NULL)) {
}

PrintPreviewHandler::~PrintPreviewHandler() {
}

void PrintPreviewHandler::RegisterMessages() {
  web_ui_->RegisterMessageCallback("getPrinters",
      NewCallback(this, &PrintPreviewHandler::HandleGetPrinters));
  web_ui_->RegisterMessageCallback("print",
      NewCallback(this, &PrintPreviewHandler::HandlePrint));
}

void PrintPreviewHandler::HandleGetPrinters(const ListValue*) {
  ListValue printers;

  printing::PrinterList printer_list;
  print_backend_->EnumeratePrinters(&printer_list);
  for (printing::PrinterList::iterator index = printer_list.begin();
       index != printer_list.end(); ++index) {
    printers.Append(new StringValue(index->printer_name));
  }

  web_ui_->CallJavascriptFunction(L"setPrinters", printers);
}

void PrintPreviewHandler::HandlePrint(const ListValue*) {
  web_ui_->GetRenderViewHost()->PrintForPrintPreview();
}
