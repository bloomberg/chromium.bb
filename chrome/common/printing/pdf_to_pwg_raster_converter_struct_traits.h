// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_PRINTING_PDF_TO_PWG_RASTER_CONVERTER_STRUCT_TRAITS_H_
#define CHROME_COMMON_PRINTING_PDF_TO_PWG_RASTER_CONVERTER_STRUCT_TRAITS_H_

#include "build/build_config.h"
#include "chrome/common/printing/pdf_to_pwg_raster_converter.mojom.h"
#include "printing/pdf_render_settings.h"
#include "printing/pwg_raster_settings.h"

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

template <>
struct EnumTraits<printing::mojom::PWGRasterSettings::TransformType,
                  printing::PwgRasterTransformType> {
  static printing::mojom::PWGRasterSettings::TransformType ToMojom(
      printing::PwgRasterTransformType transform_type) {
    switch (transform_type) {
      case printing::PwgRasterTransformType::TRANSFORM_NORMAL:
        return printing::mojom::PWGRasterSettings::TransformType::
            TRANSFORM_NORMAL;
      case printing::PwgRasterTransformType::TRANSFORM_ROTATE_180:
        return printing::mojom::PWGRasterSettings::TransformType::
            TRANSFORM_ROTATE_180;
      case printing::PwgRasterTransformType::TRANSFORM_FLIP_HORIZONTAL:
        return printing::mojom::PWGRasterSettings::TransformType::
            TRANSFORM_FLIP_HORIZONTAL;
      case printing::PwgRasterTransformType::TRANSFORM_FLIP_VERTICAL:
        return printing::mojom::PWGRasterSettings::TransformType::
            TRANSFORM_FLIP_VERTICAL;
    }
    NOTREACHED() << "Unknown transform type "
                 << static_cast<int>(transform_type);
    return printing::mojom::PWGRasterSettings::TransformType::TRANSFORM_NORMAL;
  }

  static bool FromMojom(printing::mojom::PWGRasterSettings::TransformType input,
                        printing::PwgRasterTransformType* output) {
    switch (input) {
      case printing::mojom::PWGRasterSettings::TransformType::TRANSFORM_NORMAL:
        *output = printing::PwgRasterTransformType::TRANSFORM_NORMAL;
        return true;
      case printing::mojom::PWGRasterSettings::TransformType::
          TRANSFORM_ROTATE_180:
        *output = printing::PwgRasterTransformType::TRANSFORM_ROTATE_180;
        return true;
      case printing::mojom::PWGRasterSettings::TransformType::
          TRANSFORM_FLIP_HORIZONTAL:
        *output = printing::PwgRasterTransformType::TRANSFORM_FLIP_HORIZONTAL;
        return true;
      case printing::mojom::PWGRasterSettings::TransformType::
          TRANSFORM_FLIP_VERTICAL:
        *output = printing::PwgRasterTransformType::TRANSFORM_FLIP_VERTICAL;
        return true;
    }
    NOTREACHED() << "Unknown transform type " << static_cast<int>(input);
    return false;
  }
};

template <>
class StructTraits<printing::mojom::PWGRasterSettingsDataView,
                   printing::PwgRasterSettings> {
 public:
  static bool rotate_all_pages(const printing::PwgRasterSettings& settings) {
    return settings.rotate_all_pages;
  }
  static bool reverse_page_order(const printing::PwgRasterSettings& settings) {
    return settings.reverse_page_order;
  }
  static printing::PwgRasterTransformType odd_page_transform(
      const printing::PwgRasterSettings& settings) {
    return settings.odd_page_transform;
  }

  static bool Read(printing::mojom::PWGRasterSettingsDataView data,
                   printing::PwgRasterSettings* out_settings);
};

}  // namespace mojo

#endif  // CHROME_COMMON_PRINTING_PDF_TO_PWG_RASTER_CONVERTER_STRUCT_TRAITS_H_
