// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/printing/pdf_to_pwg_raster_converter_impl.h"

#include "chrome/utility/cloud_print/bitmap_image.h"
#include "chrome/utility/cloud_print/pwg_encoder.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "pdf/pdf.h"

namespace printing {

namespace {

bool RenderPDFPagesToPWGRaster(base::File pdf_file,
                               const PdfRenderSettings& settings,
                               const PwgRasterSettings& bitmap_settings,
                               base::File bitmap_file) {
  base::File::Info info;
  if (!pdf_file.GetInfo(&info) || info.size <= 0 ||
      info.size > std::numeric_limits<int>::max())
    return false;
  int data_size = static_cast<int>(info.size);

  std::string data(data_size, 0);
  if (pdf_file.Read(0, &data[0], data_size) != data_size)
    return false;

  int total_page_count = 0;
  if (!chrome_pdf::GetPDFDocInfo(data.data(), data_size, &total_page_count,
                                 nullptr)) {
    return false;
  }

  std::string pwg_header = cloud_print::PwgEncoder::GetDocumentHeader();
  int bytes_written =
      bitmap_file.WriteAtCurrentPos(pwg_header.data(), pwg_header.size());
  if (bytes_written != static_cast<int>(pwg_header.size()))
    return false;

  cloud_print::BitmapImage image(settings.area.size(),
                                 cloud_print::BitmapImage::BGRA);
  for (int i = 0; i < total_page_count; ++i) {
    int page_number = i;

    if (bitmap_settings.reverse_page_order)
      page_number = total_page_count - 1 - page_number;

    if (!chrome_pdf::RenderPDFPageToBitmap(
            data.data(), data_size, page_number, image.pixel_data(),
            image.size().width(), image.size().height(), settings.dpi,
            settings.autorotate)) {
      return false;
    }

    cloud_print::PwgHeaderInfo header_info;
    header_info.dpi = gfx::Size(settings.dpi, settings.dpi);
    header_info.total_pages = total_page_count;

    // Transform odd pages.
    if (page_number % 2) {
      switch (bitmap_settings.odd_page_transform) {
        case TRANSFORM_NORMAL:
          break;
        case TRANSFORM_ROTATE_180:
          header_info.flipx = true;
          header_info.flipy = true;
          break;
        case TRANSFORM_FLIP_HORIZONTAL:
          header_info.flipx = true;
          break;
        case TRANSFORM_FLIP_VERTICAL:
          header_info.flipy = true;
          break;
      }
    }

    if (bitmap_settings.rotate_all_pages) {
      header_info.flipx = !header_info.flipx;
      header_info.flipy = !header_info.flipy;
    }

    std::string pwg_page =
        cloud_print::PwgEncoder::EncodePage(image, header_info);
    if (pwg_page.empty())
      return false;
    bytes_written =
        bitmap_file.WriteAtCurrentPos(pwg_page.data(), pwg_page.size());
    if (bytes_written != static_cast<int>(pwg_page.size()))
      return false;
  }
  return true;
}

}  // namespace

PDFToPWGRasterConverterImpl::PDFToPWGRasterConverterImpl(
    std::unique_ptr<service_manager::ServiceContextRef> service_ref)
    : service_ref_(std::move(service_ref)) {}

PDFToPWGRasterConverterImpl::~PDFToPWGRasterConverterImpl() {}

void PDFToPWGRasterConverterImpl::Convert(
    mojo::ScopedHandle pdf_file_in,
    const PdfRenderSettings& pdf_settings,
    const PwgRasterSettings& pwg_raster_settings,
    mojo::ScopedHandle pwg_raster_file_out,
    ConvertCallback callback) {
  base::PlatformFile pdf_file;
  if (mojo::UnwrapPlatformFile(std::move(pdf_file_in), &pdf_file) !=
      MOJO_RESULT_OK) {
    LOG(ERROR) << "Invalid PDF file passed to PDFToPWGRasterConverterImpl";
    std::move(callback).Run(false);
    return;
  }

  base::PlatformFile pwg_raster_file;
  if (mojo::UnwrapPlatformFile(std::move(pwg_raster_file_out),
                               &pwg_raster_file) != MOJO_RESULT_OK) {
    LOG(ERROR)
        << "Invalid PWGRaster file passed to PDFToPWGRasterConverterImpl";
    std::move(callback).Run(false);
    return;
  }

  bool result = RenderPDFPagesToPWGRaster(base::File(pdf_file), pdf_settings,
                                          pwg_raster_settings,
                                          base::File(pwg_raster_file));
  std::move(callback).Run(result);
}

}  // namespace printing
