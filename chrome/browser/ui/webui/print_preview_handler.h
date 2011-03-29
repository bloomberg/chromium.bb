// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_HANDLER_H_
#pragma once

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/webui/web_ui.h"

class EnumeratePrintersTaskProxy;

namespace printing {
class PrintBackend;
}

// The handler for Javascript messages related to the "print preview" dialog.
class PrintPreviewHandler : public WebUIMessageHandler,
                            public base::SupportsWeakPtr<PrintPreviewHandler> {
 public:
  PrintPreviewHandler();
  virtual ~PrintPreviewHandler();

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages();

 private:
  friend class EnumeratePrintersTaskProxy;

  // Get the list of printers. |args| is unused.
  void HandleGetPrinters(const ListValue* args);

  // Ask the initiator renderer to generate a preview.
  // First element of |args| is a job settings JSON string.
  void HandleGetPreview(const ListValue* args);

  // Get the job settings from Web UI and initiate printing.
  // First element of |args| is a job settings JSON string.
  void HandlePrint(const ListValue* args);

  // Send the list of printers to the Web UI.
  void SendPrinterList(const ListValue& printers);

  // Helper function to process the color setting in the dictionary.
  void ProcessColorSetting(const DictionaryValue& settings);

  // Helper function to process the landscape setting in the dictionary.
  void ProcessLandscapeSetting(const DictionaryValue& settings);

  // Pointer to current print system.
  scoped_refptr<printing::PrintBackend> print_backend_;

  // Set to true if we need to generate a new print preview.
  bool need_to_generate_preview_;

  // Set to true if the preview should be in color.
  bool color_;

  // Set to true if the preview should be landscape.
  bool landscape_;

  DISALLOW_COPY_AND_ASSIGN(PrintPreviewHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_HANDLER_H_
