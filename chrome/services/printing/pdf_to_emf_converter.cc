// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/printing/pdf_to_emf_converter.h"

#include <algorithm>

#include "base/lazy_instance.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "pdf/pdf.h"
#include "printing/emf_win.h"
#include "ui/gfx/gdi_util.h"

namespace printing {

namespace {

base::LazyInstance<std::vector<mojom::PdfToEmfConverterClientPtr>>::Leaky
    g_converter_clients = LAZY_INSTANCE_INITIALIZER;

void PreCacheFontCharacters(const LOGFONT* logfont,
                            const wchar_t* text,
                            size_t text_length) {
  if (g_converter_clients.Get().empty()) {
    NOTREACHED()
        << "PreCacheFontCharacters when no converter client is registered.";
    return;
  }

  // We pass the LOGFONT as an array of bytes for simplicity (no typemaps
  // required).
  std::vector<uint8_t> logfont_mojo(sizeof(LOGFONT));
  memcpy(logfont_mojo.data(), logfont, sizeof(LOGFONT));

  g_converter_clients.Get().front()->PreCacheFontCharacters(
      logfont_mojo, base::string16(text, text_length));
}

void OnConvertedClientDisconnected() {
  // We have no direct way of tracking which PdfToEmfConverterClientPtr got
  // disconnected as it is a movable type, short of using a wrapper.
  // Just traverse the list of clients and remove the ones that are not bound.
  std::remove_if(g_converter_clients.Get().begin(),
                 g_converter_clients.Get().end(),
                 [](const mojom::PdfToEmfConverterClientPtr& client) {
                   return !client.is_bound();
                 });
}

void RegisterConverterClient(mojom::PdfToEmfConverterClientPtr client) {
  if (!g_converter_clients.IsCreated()) {
    // First time this method is called.
    chrome_pdf::SetPDFEnsureTypefaceCharactersAccessible(
        PreCacheFontCharacters);
  }
  client.set_connection_error_handler(
      base::BindOnce(&OnConvertedClientDisconnected));
  g_converter_clients.Get().push_back(std::move(client));
}

}  // namespace

PdfToEmfConverter::PdfToEmfConverter(
    mojo::ScopedHandle pdf_file_in,
    const printing::PdfRenderSettings& pdf_render_settings,
    mojom::PdfToEmfConverterClientPtr client)
    : pdf_render_settings_(pdf_render_settings) {
  RegisterConverterClient(std::move(client));

  chrome_pdf::SetPDFUseGDIPrinting(pdf_render_settings_.mode ==
                                   PdfRenderSettings::Mode::GDI_TEXT);
  int printing_mode;
  switch (pdf_render_settings_.mode) {
    case PdfRenderSettings::Mode::TEXTONLY:
      printing_mode = chrome_pdf::PrintingMode::kTextOnly;
      break;
    case PdfRenderSettings::Mode::POSTSCRIPT_LEVEL2:
      printing_mode = chrome_pdf::PrintingMode::kPostScript2;
      break;
    case PdfRenderSettings::Mode::POSTSCRIPT_LEVEL3:
      printing_mode = chrome_pdf::PrintingMode::kPostScript3;
      break;
    default:
      // Not using postscript or text only.
      printing_mode = chrome_pdf::PrintingMode::kEmf;
  }
  chrome_pdf::SetPDFUsePrintMode(printing_mode);

  base::PlatformFile pdf_file;
  if (mojo::UnwrapPlatformFile(std::move(pdf_file_in), &pdf_file) !=
      MOJO_RESULT_OK) {
    LOG(ERROR) << "Invalid PDF file passed to PdfToEmfConverter.";
    return;
  }
  LoadPdf(base::File(pdf_file));
}

PdfToEmfConverter::~PdfToEmfConverter() = default;

void PdfToEmfConverter::LoadPdf(base::File pdf_file) {
  int64_t length64 = pdf_file.GetLength();
  if (length64 <= 0 || length64 > std::numeric_limits<int>::max())
    return;

  int length = static_cast<int>(length64);
  pdf_data_.resize(length);
  if (length != pdf_file.Read(0, pdf_data_.data(), pdf_data_.size()))
    return;

  int page_count = 0;
  chrome_pdf::GetPDFDocInfo(&pdf_data_.front(), pdf_data_.size(), &page_count,
                            nullptr);
  total_page_count_ = page_count;
}

bool PdfToEmfConverter::RenderPdfPageToMetafile(int page_number,
                                                base::File output_file,
                                                float* scale_factor,
                                                bool postscript) {
  Emf metafile;
  metafile.Init();

  // We need to scale down DC to fit an entire page into DC available area.
  // Current metafile is based on screen DC and have current screen size.
  // Writing outside of those boundaries will result in the cut-off output.
  // On metafiles (this is the case here), scaling down will still record
  // original coordinates and we'll be able to print in full resolution.
  // Before playback we'll need to counter the scaling up that will happen
  // in the service (print_system_win.cc).
  //
  // The postscript driver does not use the metafile size since it outputs
  // postscript rather than a metafile. Instead it uses the printable area
  // sent to RenderPDFPageToDC to determine the area to render. Therefore,
  // don't scale the DC to match the metafile, and send the printer physical
  // offsets to the driver.
  if (!postscript) {
    *scale_factor = gfx::CalculatePageScale(metafile.context(),
                                            pdf_render_settings_.area.right(),
                                            pdf_render_settings_.area.bottom());
    gfx::ScaleDC(metafile.context(), *scale_factor);
  }

  // The underlying metafile is of type Emf and ignores the arguments passed
  // to StartPage.
  metafile.StartPage(gfx::Size(), gfx::Rect(), 1);
  int offset_x = postscript ? pdf_render_settings_.offsets.x() : 0;
  int offset_y = postscript ? pdf_render_settings_.offsets.y() : 0;

  if (!chrome_pdf::RenderPDFPageToDC(
          &pdf_data_.front(), pdf_data_.size(), page_number, metafile.context(),
          pdf_render_settings_.dpi.width(), pdf_render_settings_.dpi.height(),
          pdf_render_settings_.area.x() - offset_x,
          pdf_render_settings_.area.y() - offset_y,
          pdf_render_settings_.area.width(), pdf_render_settings_.area.height(),
          true, false, true, true, pdf_render_settings_.autorotate)) {
    return false;
  }
  metafile.FinishPage();
  metafile.FinishDocument();
  return metafile.SaveTo(&output_file);
}

void PdfToEmfConverter::ConvertPage(uint32_t page_number,
                                    mojo::ScopedHandle emf_file_out,
                                    ConvertPageCallback callback) {
  if (page_number >= total_page_count_) {
    std::move(callback).Run(/*success=*/false, /*scale_factor=*/0.0);
    return;
  }

  base::PlatformFile emf_file;
  if (mojo::UnwrapPlatformFile(std::move(emf_file_out), &emf_file) !=
      MOJO_RESULT_OK) {
    std::move(callback).Run(/*success=*/false, /*scale_factor=*/0.0);
    return;
  }

  float scale_factor = 1.0f;
  bool postscript =
      pdf_render_settings_.mode == PdfRenderSettings::Mode::POSTSCRIPT_LEVEL2 ||
      pdf_render_settings_.mode == PdfRenderSettings::Mode::POSTSCRIPT_LEVEL3;
  bool success = RenderPdfPageToMetafile(page_number, base::File(emf_file),
                                         &scale_factor, postscript);
  std::move(callback).Run(success, scale_factor);
}

}  // namespace printing
