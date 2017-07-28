// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/printer_handler.h"

#include "build/buildflag.h"
#include "chrome/browser/ui/webui/print_preview/extension_printer_handler.h"
#include "chrome/common/features.h"

#if BUILDFLAG(ENABLE_SERVICE_DISCOVERY)
#include "chrome/browser/ui/webui/print_preview/privet_printer_handler.h"
#endif

// static
std::unique_ptr<PrinterHandler> PrinterHandler::CreateForExtensionPrinters(
    Profile* profile) {
  return base::MakeUnique<ExtensionPrinterHandler>(profile);
}

#if BUILDFLAG(ENABLE_SERVICE_DISCOVERY)
// static
std::unique_ptr<PrinterHandler> PrinterHandler::CreateForPrivetPrinters(
    Profile* profile) {
  return base::MakeUnique<PrivetPrinterHandler>(profile);
}
#endif
