// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UTILITY_PRINTING_PDF_TO_EMF_CONVERTER_IMPL_H_
#define CHROME_UTILITY_PRINTING_PDF_TO_EMF_CONVERTER_IMPL_H_

#include <vector>

#include "base/files/file.h"
#include "base/macros.h"
#include "chrome/common/printing/pdf_to_emf_converter.mojom.h"
#include "printing/pdf_render_settings.h"

namespace printing {

class PdfToEmfConverterImpl : public mojom::PdfToEmfConverter {
 public:
  PdfToEmfConverterImpl(mojo::ScopedHandle pdf_file_in,
                        const PdfRenderSettings& render_settings,
                        mojom::PdfToEmfConverterClientPtr client);
  ~PdfToEmfConverterImpl() override;

  int total_page_count() const { return total_page_count_; }

 private:
  // mojom::PdfToEmfConverter implementation.
  void ConvertPage(uint32_t page_number,
                   mojo::ScopedHandle emf_file_out,
                   ConvertPageCallback callback) override;

  void LoadPdf(base::File pdf_file);
  bool RenderPdfPageToMetafile(int page_number,
                               base::File output_file,
                               float* scale_factor,
                               bool postscript);

  uint32_t total_page_count_ = 0;
  PdfRenderSettings pdf_render_settings_;
  std::vector<char> pdf_data_;

  DISALLOW_COPY_AND_ASSIGN(PdfToEmfConverterImpl);
};

}  // namespace printing

#endif  // CHROME_UTILITY_PRINTING_PDF_TO_EMF_CONVERTER_IMPL_H_
