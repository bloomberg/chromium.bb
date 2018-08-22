// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/printing/pdf_nup_converter.h"

#include <string>
#include <utility>

#include "base/containers/span.h"
#include "base/logging.h"
#include "components/crash/core/common/crash_key.h"
#include "components/services/pdf_compositor/public/cpp/pdf_service_mojo_utils.h"
#include "pdf/pdf.h"

namespace printing {

namespace {

// |pdf_mappings| which has the pdf page data needs to remain valid until
// ConvertPdfPagesToNupPdf completes, since base::span is a reference type.
std::vector<base::span<const uint8_t>> CreatePdfPagesVector(
    const std::vector<base::ReadOnlySharedMemoryRegion>& pdf_page_regions,
    std::vector<base::ReadOnlySharedMemoryMapping>* pdf_mappings) {
  std::vector<base::span<const uint8_t>> pdf_page_span;

  for (auto& pdf_page_region : pdf_page_regions) {
    base::ReadOnlySharedMemoryMapping pdf_mapping = pdf_page_region.Map();
    base::span<const uint8_t> pdf_page =
        base::make_span(reinterpret_cast<const uint8_t*>(pdf_mapping.memory()),
                        pdf_mapping.size());
    pdf_page_span.push_back(pdf_page);
    pdf_mappings->push_back(std::move(pdf_mapping));
  }

  return pdf_page_span;
}

}  // namespace

PdfNupConverter::PdfNupConverter(
    std::unique_ptr<service_manager::ServiceContextRef> service_ref)
    : service_ref_(std::move(service_ref)) {}

PdfNupConverter::~PdfNupConverter() {}

void PdfNupConverter::NupPageConvert(
    uint32_t pages_per_sheet,
    const gfx::Size& page_size,
    std::vector<base::ReadOnlySharedMemoryRegion> pdf_page_regions,
    NupPageConvertCallback callback) {
  std::vector<base::ReadOnlySharedMemoryMapping> pdf_mappings;
  std::vector<base::span<const uint8_t>> input_pdf_buffers =
      CreatePdfPagesVector(pdf_page_regions, &pdf_mappings);

  void* output_pdf_buffer = nullptr;
  size_t output_pdf_buffer_size = 0;
  base::MappedReadOnlyRegion region_mapping;
  if (!chrome_pdf::ConvertPdfPagesToNupPdf(
          std::move(input_pdf_buffers), pages_per_sheet, page_size.width(),
          page_size.height(), &output_pdf_buffer, &output_pdf_buffer_size)) {
    std::move(callback).Run(mojom::PdfNupConverter::Status::CONVERSION_FAILURE,
                            std::move(region_mapping.region));
    return;
  }

  region_mapping = CreateReadOnlySharedMemoryRegion(output_pdf_buffer_size);
  memcpy(region_mapping.mapping.memory(), output_pdf_buffer,
         output_pdf_buffer_size);
  free(output_pdf_buffer);
  std::move(callback).Run(mojom::PdfNupConverter::Status::SUCCESS,
                          std::move(region_mapping.region));
}

void PdfNupConverter::NupDocumentConvert(
    uint32_t pages_per_sheet,
    const gfx::Size& page_size,
    base::ReadOnlySharedMemoryRegion src_pdf_region,
    NupDocumentConvertCallback callback) {
  base::ReadOnlySharedMemoryMapping pdf_document_mapping = src_pdf_region.Map();
  base::span<const uint8_t> input_pdf_buffer = base::make_span(
      static_cast<const uint8_t*>(pdf_document_mapping.memory()),
      pdf_document_mapping.size());
  void* output_pdf_buffer = nullptr;
  size_t output_pdf_buffer_size = 0;
  base::MappedReadOnlyRegion region_mapping;
  if (!chrome_pdf::ConvertPdfDocumentToNupPdf(
          input_pdf_buffer, pages_per_sheet, page_size.width(),
          page_size.height(), &output_pdf_buffer, &output_pdf_buffer_size)) {
    std::move(callback).Run(mojom::PdfNupConverter::Status::CONVERSION_FAILURE,
                            std::move(region_mapping.region));
    return;
  }

  region_mapping = CreateReadOnlySharedMemoryRegion(output_pdf_buffer_size);
  memcpy(region_mapping.mapping.memory(), output_pdf_buffer,
         output_pdf_buffer_size);
  free(output_pdf_buffer);
  std::move(callback).Run(mojom::PdfNupConverter::Status::SUCCESS,
                          std::move(region_mapping.region));
}

void PdfNupConverter::SetWebContentsURL(const GURL& url) {
  // Record the most recent url we tried to print. This should be sufficient
  // for users using print preview by default.
  static crash_reporter::CrashKeyString<1024> crash_key("main-frame-url");
  crash_key.Set(url.spec());
}

}  // namespace printing
