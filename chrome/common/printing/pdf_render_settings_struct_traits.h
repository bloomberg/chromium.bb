// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_PRINTING_PDF_RENDER_SETTINGS_STRUCT_TRAITS_H_
#define CHROME_COMMON_PRINTING_PDF_RENDER_SETTINGS_STRUCT_TRAITS_H_

#include "build/build_config.h"
#include "chrome/common/printing/pdf_render_settings.mojom.h"
#include "printing/pdf_render_settings.h"

namespace mojo {

template <>
struct EnumTraits<printing::mojom::PDFRenderSettings::Mode,
                  printing::PdfRenderSettings::Mode> {
  static printing::mojom::PDFRenderSettings::Mode ToMojom(
      printing::PdfRenderSettings::Mode mode) {
    switch (mode) {
      case printing::PdfRenderSettings::Mode::NORMAL:
        return printing::mojom::PDFRenderSettings::Mode::NORMAL;
#if defined(OS_WIN)
      case printing::PdfRenderSettings::Mode::TEXTONLY:
        return printing::mojom::PDFRenderSettings::Mode::TEXTONLY;
      case printing::PdfRenderSettings::Mode::GDI_TEXT:
        return printing::mojom::PDFRenderSettings::Mode::GDI_TEXT;
      case printing::PdfRenderSettings::Mode::POSTSCRIPT_LEVEL2:
        return printing::mojom::PDFRenderSettings::Mode::POSTSCRIPT_LEVEL2;
      case printing::PdfRenderSettings::Mode::POSTSCRIPT_LEVEL3:
        return printing::mojom::PDFRenderSettings::Mode::POSTSCRIPT_LEVEL3;
#endif
    }
    NOTREACHED() << "Unknown mode " << static_cast<int>(mode);
    return printing::mojom::PDFRenderSettings::Mode::NORMAL;
  }

  static bool FromMojom(printing::mojom::PDFRenderSettings::Mode input,
                        printing::PdfRenderSettings::Mode* output) {
    switch (input) {
      case printing::mojom::PDFRenderSettings::Mode::NORMAL:
        *output = printing::PdfRenderSettings::Mode::NORMAL;
        return true;
#if defined(OS_WIN)
      case printing::mojom::PDFRenderSettings::Mode::TEXTONLY:
        *output = printing::PdfRenderSettings::Mode::TEXTONLY;
        return true;
      case printing::mojom::PDFRenderSettings::Mode::GDI_TEXT:
        *output = printing::PdfRenderSettings::Mode::GDI_TEXT;
        return true;
      case printing::mojom::PDFRenderSettings::Mode::POSTSCRIPT_LEVEL2:
        *output = printing::PdfRenderSettings::Mode::POSTSCRIPT_LEVEL2;
        return true;
      case printing::mojom::PDFRenderSettings::Mode::POSTSCRIPT_LEVEL3:
        *output = printing::PdfRenderSettings::Mode::POSTSCRIPT_LEVEL3;
        return true;
#else
      case printing::mojom::PDFRenderSettings::Mode::TEXTONLY:
      case printing::mojom::PDFRenderSettings::Mode::GDI_TEXT:
      case printing::mojom::PDFRenderSettings::Mode::POSTSCRIPT_LEVEL2:
      case printing::mojom::PDFRenderSettings::Mode::POSTSCRIPT_LEVEL3:
        NOTREACHED() << "Unsupported mode " << static_cast<int>(input)
                     << " on non Windows platform ";
        return false;
#endif
    }
    NOTREACHED() << "Unknown mode " << static_cast<int>(input);
    return false;
  }
};

template <>
class StructTraits<printing::mojom::PDFRenderSettingsDataView,
                   printing::PdfRenderSettings> {
 public:
  static gfx::Rect area(const printing::PdfRenderSettings& settings) {
    return settings.area;
  }
  static gfx::Point offsets(const printing::PdfRenderSettings& settings) {
    return settings.offsets;
  }
  static int32_t dpi(const printing::PdfRenderSettings& settings) {
    return settings.dpi;
  }
  static bool autorotate(const printing::PdfRenderSettings& settings) {
    return settings.autorotate;
  }
  static printing::PdfRenderSettings::Mode mode(
      const printing::PdfRenderSettings& settings) {
    return settings.mode;
  }

  static bool Read(printing::mojom::PDFRenderSettingsDataView data,
                   printing::PdfRenderSettings* out_settings);
};

}  // namespace mojo

#endif  // CHROME_COMMON_PRINTING_PDF_RENDER_SETTINGS_STRUCT_TRAITS_H_
