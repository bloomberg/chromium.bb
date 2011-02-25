// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBUI_PRINT_PREVIEW_HANDLER_H_
#define CHROME_BROWSER_WEBUI_PRINT_PREVIEW_HANDLER_H_
#pragma once

#include "base/ref_counted.h"
#include "base/weak_ptr.h"
#include "content/browser/webui/web_ui.h"

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
  // Get the list of printers and send it to the Web UI. |args| is unused.
  void HandleGetPrinters(const ListValue* args);

  // Print the preview PDF. |args| is unused.
  void HandlePrint(const ListValue* args);

  // Pointer to current print system.
  scoped_refptr<printing::PrintBackend> print_backend_;

  DISALLOW_COPY_AND_ASSIGN(PrintPreviewHandler);
};

#endif  // CHROME_BROWSER_WEBUI_PRINT_PREVIEW_HANDLER_H_
