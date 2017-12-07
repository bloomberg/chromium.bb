// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UTILITY_PRINTING_PDF_TO_EMF_CONVERTER_FACTORY_IMPL_H_
#define CHROME_UTILITY_PRINTING_PDF_TO_EMF_CONVERTER_FACTORY_IMPL_H_

#include "base/macros.h"
#include "chrome/common/printing/pdf_to_emf_converter.mojom.h"

namespace printing {

class PdfToEmfConverterFactoryImpl : public mojom::PdfToEmfConverterFactory {
 public:
  PdfToEmfConverterFactoryImpl();
  ~PdfToEmfConverterFactoryImpl() override;

  static void Create(mojom::PdfToEmfConverterFactoryRequest request);

 private:
  // mojom::PdfToEmfConverterFactory implementation.
  void CreateConverter(mojo::ScopedHandle pdf_file_in,
                       const printing::PdfRenderSettings& render_settings,
                       mojom::PdfToEmfConverterClientPtr client,
                       CreateConverterCallback callback) override;

  DISALLOW_COPY_AND_ASSIGN(PdfToEmfConverterFactoryImpl);
};

}  // namespace printing

#endif  // CHROME_UTILITY_PRINTING_PDF_TO_EMF_CONVERTER_FACTORY_IMPL_H_
