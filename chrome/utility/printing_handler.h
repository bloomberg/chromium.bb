// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UTILITY_PRINTING_HANDLER_H_
#define CHROME_UTILITY_PRINTING_HANDLER_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/utility/utility_message_handler.h"
#include "printing/features/features.h"

#if !BUILDFLAG(ENABLE_PRINT_PREVIEW) && \
    !(BUILDFLAG(ENABLE_BASIC_PRINTING) && defined(OS_WIN))
#error "Windows basic printing or print preview must be enabled"
#endif

namespace printing {

// Dispatches IPCs for printing.
class PrintingHandler : public UtilityMessageHandler {
 public:
  PrintingHandler();
  ~PrintingHandler() override;

  // IPC::Listener:
  bool OnMessageReceived(const IPC::Message& message) override;

 private:
  // IPC message handlers.
#if defined(OS_WIN)
  void OnRenderPDFPagesToMetafileStop();
#endif  // OS_WIN
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  void OnGetPrinterCapsAndDefaults(const std::string& printer_name);
  void OnGetPrinterSemanticCapsAndDefaults(const std::string& printer_name);
#endif  // ENABLE_PRINT_PREVIEW

  DISALLOW_COPY_AND_ASSIGN(PrintingHandler);
};

}  // namespace printing

#endif  // CHROME_UTILITY_PRINTING_HANDLER_H_
