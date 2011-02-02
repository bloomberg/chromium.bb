// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOM_UI_PRINT_PREVIEW_HANDLER_H_
#define CHROME_BROWSER_DOM_UI_PRINT_PREVIEW_HANDLER_H_
#pragma once

#include "base/scoped_ptr.h"
#include "base/weak_ptr.h"
#include "chrome/browser/dom_ui/dom_ui.h"

namespace printing {
class PrintBackend;
}  // namespace printing

// The handler for Javascript messages related to the "print preview" dialog.
class PrintPreviewHandler : public DOMMessageHandler,
                            public base::SupportsWeakPtr<PrintPreviewHandler> {
 public:
  PrintPreviewHandler();
  virtual ~PrintPreviewHandler();

  // DOMMessageHandler implementation.
  virtual void RegisterMessages();

 private:
  // Get the PDF preview and refresh the PDF plugin. |args| is unused.
  void HandleGetPreview(const ListValue* args);

  // Get the list of printers and send it to the Web UI. |args| is unused.
  void HandleGetPrinters(const ListValue* args);

  // Pointer to current print system.
  scoped_refptr<printing::PrintBackend> print_backend_;

  DISALLOW_COPY_AND_ASSIGN(PrintPreviewHandler);
};

#endif  // CHROME_BROWSER_DOM_UI_PRINT_PREVIEW_HANDLER_H_
