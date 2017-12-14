// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_PRINTING_PDF_TO_PWG_RASTER_CONVERTER_H_
#define CHROME_SERVICES_PRINTING_PDF_TO_PWG_RASTER_CONVERTER_H_

#include <memory>

#include "base/macros.h"
#include "chrome/services/printing/public/interfaces/pdf_to_pwg_raster_converter.mojom.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/service_manager/public/cpp/service_context_ref.h"

namespace printing {

struct PdfRenderSettings;

class PdfToPwgRasterConverterImpl
    : public printing::mojom::PdfToPwgRasterConverter {
 public:
  explicit PdfToPwgRasterConverterImpl(
      std::unique_ptr<service_manager::ServiceContextRef> service_ref);
  ~PdfToPwgRasterConverterImpl() override;

 private:
  // printing::mojom::PdfToPwgRasterConverter
  void Convert(mojo::ScopedHandle pdf_file_in,
               const PdfRenderSettings& pdf_settings,
               const PwgRasterSettings& pwg_raster_settings,
               mojo::ScopedHandle pwg_raster_file_out,
               ConvertCallback callback) override;

  const std::unique_ptr<service_manager::ServiceContextRef> service_ref_;

  DISALLOW_COPY_AND_ASSIGN(PdfToPwgRasterConverterImpl);
};

}  // namespace printing

#endif  // CHROME_SERVICES_PRINTING_PDF_TO_PWG_RASTER_CONVERTER_H_
