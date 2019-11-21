// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/printing/test/mock_printer.h"

#include "base/files/file_util.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/printing/common/print_messages.h"
#include "ipc/ipc_message_utils.h"
#include "printing/metafile_skia.h"
#include "printing/units.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_MACOSX)
#include "printing/pdf_metafile_cg_mac.h"
#endif

namespace {

void UpdateMargins(int margins_type, int dpi, PrintMsg_Print_Params* params) {
  if (margins_type == printing::NO_MARGINS) {
    params->content_size.SetSize(static_cast<int>((8.5 * dpi)),
                                 static_cast<int>((11.0 * dpi)));
    params->margin_left = 0;
    params->margin_top = 0;
  } else if (margins_type == printing::PRINTABLE_AREA_MARGINS) {
    params->content_size.SetSize(static_cast<int>((8.0 * dpi)),
                                 static_cast<int>((10.5 * dpi)));
    params->margin_left = static_cast<int>(0.25 * dpi);
    params->margin_top = static_cast<int>(0.25 * dpi);
  } else if (margins_type == printing::CUSTOM_MARGINS) {
    params->content_size.SetSize(static_cast<int>((7.9 * dpi)),
                                 static_cast<int>((10.4 * dpi)));
    params->margin_left = static_cast<int>(0.30 * dpi);
    params->margin_top = static_cast<int>(0.30 * dpi);
  }
}

void UpdatePageSizeAndScaling(const gfx::Size& page_size,
                              int scale_factor,
                              PrintMsg_Print_Params* params) {
  params->page_size = page_size;
  params->scale_factor = static_cast<double>(scale_factor) / 100.0;
}

}  // namespace

MockPrinterPage::MockPrinterPage(const void* source_data,
                                 uint32_t source_size,
                                 const printing::Image& image)
    : source_size_(source_size), image_(image) {
  // Create copies of the source data
  source_data_.reset(new uint8_t[source_size]);
  if (source_data_.get())
    memcpy(source_data_.get(), source_data, source_size);
}

MockPrinterPage::~MockPrinterPage() {
}

MockPrinter::MockPrinter()
    : dpi_(printing::kPointsPerInch),
      selection_only_(false),
      should_print_backgrounds_(false),
      document_cookie_(-1),
      current_document_cookie_(0),
      printer_status_(PRINTER_READY),
      number_pages_(0),
      page_number_(0),
      is_first_request_(true),
      print_to_pdf_(false),
      preview_request_id_(0),
      print_scaling_option_(blink::kWebPrintScalingOptionSourceSize),
      display_header_footer_(false),
      title_(base::ASCIIToUTF16("title")),
      url_(base::ASCIIToUTF16("url")),
      use_invalid_settings_(false) {
  page_size_.SetSize(static_cast<int>(8.5 * dpi_),
                     static_cast<int>(11.0 * dpi_));
  content_size_.SetSize(static_cast<int>((7.5 * dpi_)),
                        static_cast<int>((10.0 * dpi_)));
  margin_left_ = margin_top_ = static_cast<int>(0.5 * dpi_);
  printable_area_.SetRect(
      static_cast<int>(0.25 * dpi_), static_cast<int>(0.25 * dpi_),
      static_cast<int>(8 * dpi_), static_cast<int>(10.5 * dpi_));
}

MockPrinter::~MockPrinter() {
}

void MockPrinter::ResetPrinter() {
  printer_status_ = PRINTER_READY;
  document_cookie_ = -1;
}

void MockPrinter::GetDefaultPrintSettings(PrintMsg_Print_Params* params) {
  // Verify this printer is not processing a job.
  // Sorry, this mock printer is very fragile.
  EXPECT_EQ(-1, document_cookie_);

  // Assign a unit document cookie and set the print settings.
  document_cookie_ = CreateDocumentCookie();
  params->Reset();
  SetPrintParams(params);
}

void MockPrinter::SetDefaultPrintSettings(const PrintMsg_Print_Params& params) {
  // Use the same logic as in printing/print_settings.h
  dpi_ = std::max(params.dpi.width(), params.dpi.height());
  selection_only_ = params.selection_only;
  should_print_backgrounds_ = params.should_print_backgrounds;
  page_size_ = params.page_size;
  content_size_ = params.content_size;
  printable_area_ = params.printable_area;
  margin_left_ = params.margin_left;
  margin_top_ = params.margin_top;
  display_header_footer_ = params.display_header_footer;
  title_ = params.title;
  url_ = params.url;
}

void MockPrinter::UseInvalidSettings() {
  use_invalid_settings_ = true;
  PrintMsg_Print_Params empty_param;
  SetDefaultPrintSettings(empty_param);
}

void MockPrinter::UseInvalidPageSize() {
  page_size_.SetSize(0, 0);
}

void MockPrinter::UseInvalidContentSize() {
  content_size_.SetSize(0, 0);
}

void MockPrinter::ScriptedPrint(int cookie,
                                int expected_pages_count,
                                bool has_selection,
                                PrintMsg_PrintPages_Params* settings) {
  // Verify the input parameters.
  EXPECT_EQ(document_cookie_, cookie);

  settings->Reset();

  settings->params.dpi = gfx::Size(dpi_, dpi_);
  settings->params.selection_only = selection_only_;
  settings->params.should_print_backgrounds = should_print_backgrounds_;
  settings->params.document_cookie = document_cookie_;
  settings->params.page_size = page_size_;
  settings->params.content_size = content_size_;
  settings->params.printable_area = printable_area_;
  settings->params.is_first_request = is_first_request_;
  settings->params.print_scaling_option = print_scaling_option_;
  settings->params.print_to_pdf = print_to_pdf_;
  settings->params.preview_request_id = preview_request_id_;
  settings->params.display_header_footer = display_header_footer_;
  settings->params.title = title_;
  settings->params.url = url_;
  printer_status_ = PRINTER_PRINTING;
}

void MockPrinter::UpdateSettings(int cookie,
                                 PrintMsg_PrintPages_Params* params,
                                 const std::vector<int>& pages,
                                 int margins_type,
                                 const gfx::Size& page_size,
                                 int scale_factor) {
  if (document_cookie_ == -1) {
    document_cookie_ = CreateDocumentCookie();
  }
  params->Reset();
  params->pages = pages;
  SetPrintParams(&(params->params));
  UpdateMargins(margins_type, dpi_, &(params->params));
  if (!page_size.IsEmpty())
    UpdatePageSizeAndScaling(page_size, scale_factor, &params->params);
  printer_status_ = PRINTER_PRINTING;
}

void MockPrinter::SetPrintedPagesCount(int cookie, int number_pages) {
  // Verify the input parameter and update the printer status so that the
  // RenderViewTest class can verify the this function finishes without errors.
  EXPECT_EQ(document_cookie_, cookie);
  EXPECT_EQ(PRINTER_PRINTING, printer_status_);
  EXPECT_EQ(0, number_pages_);
  EXPECT_EQ(0, page_number_);

  // Initialize the job status.
  number_pages_ = number_pages;
  page_number_ = 0;
  pages_.clear();
}

void MockPrinter::PrintPage(
    const PrintHostMsg_DidPrintDocument_Params& params) {
  // Verify the input parameter and update the printer status so that the
  // RenderViewTest class can verify the this function finishes without errors.
  EXPECT_EQ(PRINTER_PRINTING, printer_status_);
  EXPECT_EQ(document_cookie_, params.document_cookie);

#if defined(OS_WIN) || defined(OS_MACOSX)
  // Load the data sent from a RenderView object and create a PageData object.
  ASSERT_TRUE(params.content.metafile_data_region.IsValid());
  base::ReadOnlySharedMemoryMapping mapping =
      params.content.metafile_data_region.Map();
  ASSERT_TRUE(mapping.IsValid());
  EXPECT_GT(mapping.size(), 0U);

#if defined(OS_MACOSX)
  printing::PdfMetafileCg metafile;
#else
  printing::MetafileSkia metafile;
#endif
  metafile.InitFromData(mapping.memory(), mapping.size());
  printing::Image image(metafile);
  pages_.push_back(base::MakeRefCounted<MockPrinterPage>(
      mapping.memory(), mapping.size(), image));
#endif

  // We finish printing a printing job.
  // Reset the job status and the printer status.
  ++page_number_;
  if (number_pages_ == page_number_)
    ResetPrinter();
}

int MockPrinter::GetPrintedPages() const {
  if (printer_status_ != PRINTER_READY)
    return -1;
  return page_number_;
}

const MockPrinterPage* MockPrinter::GetPrintedPage(unsigned int pageno) const {
  if (pageno >= pages_.size())
    return nullptr;
  return pages_[pageno].get();
}

int MockPrinter::GetWidth(unsigned int page) const {
  if (printer_status_ != PRINTER_READY || page >= pages_.size())
    return -1;
  return pages_[page]->width();
}

int MockPrinter::GetHeight(unsigned int page) const {
  if (printer_status_ != PRINTER_READY || page >= pages_.size())
    return -1;
  return pages_[page]->height();
}

bool MockPrinter::GetBitmapChecksum(unsigned int page,
                                    std::string* checksum) const {
  if (printer_status_ != PRINTER_READY || page >= pages_.size())
    return false;
  *checksum = pages_[page]->image().checksum();
  return true;
}

bool MockPrinter::SaveSource(unsigned int page,
                             const base::FilePath& filepath) const {
  if (printer_status_ != PRINTER_READY || page >= pages_.size())
    return false;
  const uint8_t* source_data = pages_[page]->source_data();
  uint32_t source_size = pages_[page]->source_size();
  base::WriteFile(filepath, reinterpret_cast<const char*>(source_data),
                  source_size);
  return true;
}

bool MockPrinter::SaveBitmap(unsigned int page,
                             const base::FilePath& filepath) const {
  if (printer_status_ != PRINTER_READY || page >= pages_.size())
    return false;

  pages_[page]->image().SaveToPng(filepath);
  return true;
}

int MockPrinter::CreateDocumentCookie() {
  return use_invalid_settings_ ? 0 : ++current_document_cookie_;
}

void MockPrinter::SetPrintParams(PrintMsg_Print_Params* params) {
  params->dpi = gfx::Size(dpi_, dpi_);
  params->selection_only = selection_only_;
  params->should_print_backgrounds = should_print_backgrounds_;
  params->document_cookie = document_cookie_;
  params->page_size = page_size_;
  params->content_size = content_size_;
  params->printable_area = printable_area_;
  params->margin_left = margin_left_;
  params->margin_top = margin_top_;
  params->is_first_request = is_first_request_;
  params->print_scaling_option = print_scaling_option_;
  params->print_to_pdf = print_to_pdf_;
  params->preview_request_id = preview_request_id_;
  params->display_header_footer = display_header_footer_;
  params->title = title_;
  params->url = url_;
}
