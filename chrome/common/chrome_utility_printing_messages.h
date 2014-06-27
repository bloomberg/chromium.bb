// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included message file, so no include guard.

#include <string>
#include <vector>

#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_platform_file.h"
#include "printing/backend/print_backend.h"
#include "printing/page_range.h"
#include "printing/pdf_render_settings.h"
#include "printing/pwg_raster_settings.h"

#if !defined(ENABLE_FULL_PRINTING)
#error "Full printing must be enabled"
#endif

#define IPC_MESSAGE_START ChromeUtilityPrintingMsgStart

IPC_STRUCT_TRAITS_BEGIN(printing::PageRange)
  IPC_STRUCT_TRAITS_MEMBER(from)
  IPC_STRUCT_TRAITS_MEMBER(to)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(printing::PrinterCapsAndDefaults)
  IPC_STRUCT_TRAITS_MEMBER(printer_capabilities)
  IPC_STRUCT_TRAITS_MEMBER(caps_mime_type)
  IPC_STRUCT_TRAITS_MEMBER(printer_defaults)
  IPC_STRUCT_TRAITS_MEMBER(defaults_mime_type)
IPC_STRUCT_TRAITS_END()

IPC_ENUM_TRAITS_MAX_VALUE(printing::ColorModel, printing::PROCESSCOLORMODEL_RGB)
IPC_ENUM_TRAITS_MIN_MAX_VALUE(printing::DuplexMode,
                              printing::UNKNOWN_DUPLEX_MODE,
                              printing::SHORT_EDGE)

IPC_STRUCT_TRAITS_BEGIN(printing::PrinterSemanticCapsAndDefaults::Paper)
  IPC_STRUCT_TRAITS_MEMBER(display_name)
  IPC_STRUCT_TRAITS_MEMBER(vendor_id)
  IPC_STRUCT_TRAITS_MEMBER(size_um)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(printing::PrinterSemanticCapsAndDefaults)
  IPC_STRUCT_TRAITS_MEMBER(collate_capable)
  IPC_STRUCT_TRAITS_MEMBER(collate_default)
  IPC_STRUCT_TRAITS_MEMBER(copies_capable)
  IPC_STRUCT_TRAITS_MEMBER(duplex_capable)
  IPC_STRUCT_TRAITS_MEMBER(duplex_default)
  IPC_STRUCT_TRAITS_MEMBER(color_changeable)
  IPC_STRUCT_TRAITS_MEMBER(color_default)
  IPC_STRUCT_TRAITS_MEMBER(color_model)
  IPC_STRUCT_TRAITS_MEMBER(bw_model)
  IPC_STRUCT_TRAITS_MEMBER(papers)
  IPC_STRUCT_TRAITS_MEMBER(default_paper)
  IPC_STRUCT_TRAITS_MEMBER(dpis)
  IPC_STRUCT_TRAITS_MEMBER(default_dpi)
IPC_STRUCT_TRAITS_END()

IPC_ENUM_TRAITS(printing::PwgRasterTransformType)

IPC_STRUCT_TRAITS_BEGIN(printing::PwgRasterSettings)
  IPC_STRUCT_TRAITS_MEMBER(odd_page_transform)
  IPC_STRUCT_TRAITS_MEMBER(rotate_all_pages)
  IPC_STRUCT_TRAITS_MEMBER(reverse_page_order)
IPC_STRUCT_TRAITS_END()

//------------------------------------------------------------------------------
// Utility process messages:
// These are messages from the browser to the utility process.

// Tell the utility process to render the given PDF into a PWGRaster.
IPC_MESSAGE_CONTROL4(ChromeUtilityMsg_RenderPDFPagesToPWGRaster,
                     IPC::PlatformFileForTransit, /* Input PDF file */
                     printing::PdfRenderSettings, /* PDF render settings */
                     // PWG transform settings.
                     printing::PwgRasterSettings,
                     IPC::PlatformFileForTransit /* Output PWG file */)

// Tells the utility process to get capabilities and defaults for the specified
// printer. Used on Windows to isolate the service process from printer driver
// crashes by executing this in a separate process. This does not run in a
// sandbox.
IPC_MESSAGE_CONTROL1(ChromeUtilityMsg_GetPrinterCapsAndDefaults,
                     std::string /* printer name */)

// Tells the utility process to get capabilities and defaults for the specified
// printer. Used on Windows to isolate the service process from printer driver
// crashes by executing this in a separate process. This does not run in a
// sandbox. Returns result as printing::PrinterSemanticCapsAndDefaults.
IPC_MESSAGE_CONTROL1(ChromeUtilityMsg_GetPrinterSemanticCapsAndDefaults,
                     std::string /* printer name */)


#if defined(WIN_PDF_METAFILE_FOR_PRINTING)
// Tell the utility process to render the given PDF into a metafile.
// The metafile path will have ".%d" inserted where the %d is the page number.
// If no page range is specified, all pages will be converted.
IPC_MESSAGE_CONTROL4(ChromeUtilityMsg_RenderPDFPagesToMetafiles,
                     IPC::PlatformFileForTransit,  // PDF file
                     base::FilePath,  // Base location for output metafile
                     printing::PdfRenderSettings,  // PDF render settings
                     std::vector<printing::PageRange>)
#endif

//------------------------------------------------------------------------------
// Utility process host messages:
// These are messages from the utility process to the browser.

// Reply when the utility process has succeeded in rendering the PDF to PWG.
IPC_MESSAGE_CONTROL0(ChromeUtilityHostMsg_RenderPDFPagesToPWGRaster_Succeeded)

// Reply when an error occurred rendering the PDF to PWG.
IPC_MESSAGE_CONTROL0(ChromeUtilityHostMsg_RenderPDFPagesToPWGRaster_Failed)

// Reply when the utility process has succeeded in obtaining the printer
// capabilities and defaults.
IPC_MESSAGE_CONTROL2(ChromeUtilityHostMsg_GetPrinterCapsAndDefaults_Succeeded,
                     std::string /* printer name */,
                     printing::PrinterCapsAndDefaults)

// Reply when the utility process has succeeded in obtaining the printer
// semantic capabilities and defaults.
IPC_MESSAGE_CONTROL2(
    ChromeUtilityHostMsg_GetPrinterSemanticCapsAndDefaults_Succeeded,
    std::string /* printer name */,
    printing::PrinterSemanticCapsAndDefaults)

// Reply when the utility process has failed to obtain the printer
// capabilities and defaults.
IPC_MESSAGE_CONTROL1(ChromeUtilityHostMsg_GetPrinterCapsAndDefaults_Failed,
                     std::string /* printer name */)

// Reply when the utility process has failed to obtain the printer
// semantic capabilities and defaults.
IPC_MESSAGE_CONTROL1(
  ChromeUtilityHostMsg_GetPrinterSemanticCapsAndDefaults_Failed,
  std::string /* printer name */)

#if defined(WIN_PDF_METAFILE_FOR_PRINTING)
// Reply when the utility process has succeeded in rendering the PDF.
IPC_MESSAGE_CONTROL2(ChromeUtilityHostMsg_RenderPDFPagesToMetafiles_Succeeded,
                     std::vector<printing::PageRange>,  // Pages rendered
                     double)                            // Scale factor

// Reply when an error occurred rendering the PDF.
IPC_MESSAGE_CONTROL0(ChromeUtilityHostMsg_RenderPDFPagesToMetafile_Failed)
#endif
