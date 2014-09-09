// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UTILITY_PRINTING_HANDLER_H_
#define CHROME_UTILITY_PRINTING_HANDLER_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/utility/utility_message_handler.h"
#include "ipc/ipc_platform_file.h"

#if !defined(ENABLE_FULL_PRINTING) && !defined(OS_WIN)
#error "Windows or full printing must be enabled"
#endif

namespace printing {
class PdfRenderSettings;
struct PwgRasterSettings;
struct PageRange;
}

// Dispatches IPCs for printing.
class PrintingHandler : public UtilityMessageHandler {
 public:
  PrintingHandler();
  virtual ~PrintingHandler();

  static void PreSandboxStartup();

  // IPC::Listener:
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

 private:
  // IPC message handlers.
#if defined(OS_WIN)
  void OnRenderPDFPagesToMetafile(
      IPC::PlatformFileForTransit pdf_transit,
      const base::FilePath& metafile_path,
      const printing::PdfRenderSettings& settings,
      const std::vector<printing::PageRange>& page_ranges);
#endif  // OS_WIN
#if defined(ENABLE_FULL_PRINTING)
  void OnRenderPDFPagesToPWGRaster(
      IPC::PlatformFileForTransit pdf_transit,
      const printing::PdfRenderSettings& settings,
      const printing::PwgRasterSettings& bitmap_settings,
      IPC::PlatformFileForTransit bitmap_transit);
#endif  // ENABLE_FULL_PRINTING

#if defined(OS_WIN)
  // Helper method for Windows.
  // |highest_rendered_page_number| is set to -1 on failure to render any page.
  // |page_ranges| is both input and output. If supplied as input, only the
  // specified pages will be rendered. If an empty vector is supplied it will
  // be filled with a range of all pages that were rendered.
  bool RenderPDFToWinMetafile(
      base::File pdf_file,
      const base::FilePath& metafile_path,
      const printing::PdfRenderSettings& settings,
      std::vector<printing::PageRange>* page_ranges,
      int* highest_rendered_page_number,
      double* scale_factor);
#endif  // OS_WIN
#if defined(ENABLE_FULL_PRINTING)
  bool RenderPDFPagesToPWGRaster(
      base::File pdf_file,
      const printing::PdfRenderSettings& settings,
      const printing::PwgRasterSettings& bitmap_settings,
      base::File bitmap_file);

  void OnGetPrinterCapsAndDefaults(const std::string& printer_name);
  void OnGetPrinterSemanticCapsAndDefaults(const std::string& printer_name);
#endif  // ENABLE_FULL_PRINTING

  DISALLOW_COPY_AND_ASSIGN(PrintingHandler);
};

#endif  // CHROME_UTILITY_PRINTING_HANDLER_H_
