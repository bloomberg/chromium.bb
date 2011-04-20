// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_HANDLER_H_
#pragma once

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/shell_dialogs.h"
#include "content/browser/webui/web_ui.h"

class FilePath;
class PrintSystemTaskProxy;
class FundamentalValue;

namespace printing {
class PrintBackend;
}

// The handler for Javascript messages related to the "print preview" dialog.
class PrintPreviewHandler : public WebUIMessageHandler,
                            public base::SupportsWeakPtr<PrintPreviewHandler>,
                            public SelectFileDialog::Listener {
 public:
  PrintPreviewHandler();
  virtual ~PrintPreviewHandler();

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages();

  // SelectFileDialog::Listener implementation.
  virtual void FileSelected(const FilePath& path, int index, void* params);

  // Displays a modal dialog, prompting the user to select a file.
  void SelectFile(const FilePath& default_path);

 private:
  friend class PrintSystemTaskProxy;

  // Get the list of printers. |args| is unused.
  void HandleGetPrinters(const ListValue* args);

  // Ask the initiator renderer to generate a preview.
  // First element of |args| is a job settings JSON string.
  void HandleGetPreview(const ListValue* args);

  // Get the job settings from Web UI and initiate printing.
  // First element of |args| is a job settings JSON string.
  void HandlePrint(const ListValue* args);

  // Get the printer capabilities.
  // First element of |args| is the printer name.
  void HandleGetPrinterCapabilities(const ListValue* args);

  // Send the printer capabilities to the Web UI.
  // |settings_info| contains printer capabilities information.
  void SendPrinterCapabilities(const DictionaryValue& settings_info);

  // Send the list of printers to the Web UI.
  void SendPrinterList(const ListValue& printers,
                       const FundamentalValue& default_printer_index);

  // Helper function to process the color setting in the dictionary.
  void ProcessColorSetting(const DictionaryValue& settings);

  // Helper function to process the landscape setting in the dictionary.
  void ProcessLandscapeSetting(const DictionaryValue& settings);

  // Pointer to current print system.
  scoped_refptr<printing::PrintBackend> print_backend_;

  // The underlying dialog object.
  scoped_refptr<SelectFileDialog> select_file_dialog_;

  static FilePath* last_saved_path_;

  DISALLOW_COPY_AND_ASSIGN(PrintPreviewHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_HANDLER_H_
