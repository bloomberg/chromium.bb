// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/printing_handler.h"

#include <utility>

#include "base/files/file_util.h"
#include "build/build_config.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_utility_printing_messages.h"
#include "content/public/utility/utility_thread.h"
#include "printing/features/features.h"
#include "printing/page_range.h"

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
#include "chrome/common/crash_keys.h"
#include "printing/backend/print_backend.h"
#endif

namespace printing {

namespace {

bool Send(IPC::Message* message) {
  return content::UtilityThread::Get()->Send(message);
}

void ReleaseProcess() {
  content::UtilityThread::Get()->ReleaseProcess();
}

}  // namespace

PrintingHandler::PrintingHandler() = default;

PrintingHandler::~PrintingHandler() = default;

bool PrintingHandler::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PrintingHandler, message)
#if defined(OS_WIN)
    IPC_MESSAGE_HANDLER(ChromeUtilityMsg_RenderPDFPagesToMetafiles_Stop,
                        OnRenderPDFPagesToMetafileStop)
#endif  // OS_WIN
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
    IPC_MESSAGE_HANDLER(ChromeUtilityMsg_GetPrinterCapsAndDefaults,
                        OnGetPrinterCapsAndDefaults)
    IPC_MESSAGE_HANDLER(ChromeUtilityMsg_GetPrinterSemanticCapsAndDefaults,
                        OnGetPrinterSemanticCapsAndDefaults)
#endif  // ENABLE_PRINT_PREVIEW
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

#if defined(OS_WIN)
void PrintingHandler::OnRenderPDFPagesToMetafileStop() {
  ReleaseProcess();
}
#endif

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
void PrintingHandler::OnGetPrinterCapsAndDefaults(
    const std::string& printer_name) {
  scoped_refptr<PrintBackend> print_backend =
      PrintBackend::CreateInstance(nullptr);
  PrinterCapsAndDefaults printer_info;

  crash_keys::ScopedPrinterInfo crash_key(
      print_backend->GetPrinterDriverInfo(printer_name));

  if (print_backend->GetPrinterCapsAndDefaults(printer_name, &printer_info)) {
    Send(new ChromeUtilityHostMsg_GetPrinterCapsAndDefaults_Succeeded(
        printer_name, printer_info));
  } else  {
    Send(new ChromeUtilityHostMsg_GetPrinterCapsAndDefaults_Failed(
        printer_name));
  }
  ReleaseProcess();
}

void PrintingHandler::OnGetPrinterSemanticCapsAndDefaults(
    const std::string& printer_name) {
  scoped_refptr<PrintBackend> print_backend =
      PrintBackend::CreateInstance(nullptr);
  PrinterSemanticCapsAndDefaults printer_info;

  crash_keys::ScopedPrinterInfo crash_key(
      print_backend->GetPrinterDriverInfo(printer_name));

  if (print_backend->GetPrinterSemanticCapsAndDefaults(printer_name,
                                                       &printer_info)) {
    Send(new ChromeUtilityHostMsg_GetPrinterSemanticCapsAndDefaults_Succeeded(
        printer_name, printer_info));
  } else {
    Send(new ChromeUtilityHostMsg_GetPrinterSemanticCapsAndDefaults_Failed(
        printer_name));
  }
  ReleaseProcess();
}
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)

}  // namespace printing
