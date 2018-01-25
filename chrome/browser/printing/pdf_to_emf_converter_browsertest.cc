// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/in_process_browser_test.h"

#include <Windows.h>

#include "base/files/file_util.h"
#include "base/memory/ref_counted_memory.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/sha1.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/printing/pdf_to_emf_converter.h"
#include "chrome/common/chrome_paths.h"
#include "printing/emf_win.h"
#include "printing/metafile.h"
#include "printing/pdf_render_settings.h"

namespace printing {

namespace {

using PDFToEMFConverterBrowserTest = InProcessBrowserTest;

void StartCallbackImpl(base::Closure quit_closure,
                       int* page_count_out,
                       int page_count_in) {
  *page_count_out = page_count_in;
  quit_closure.Run();
}

void GetPageCallbackImpl(base::Closure quit_closure,
                         int* page_number_out,
                         std::unique_ptr<MetafilePlayer>* file_out,
                         int page_number_in,
                         float scale_factor,
                         std::unique_ptr<MetafilePlayer> file_in) {
  *page_number_out = page_number_in;
  *file_out = std::move(file_in);
  quit_closure.Run();
}

std::unique_ptr<ENHMETAHEADER> GetEMFHeader(const std::string& emf_data) {
  Emf emf;
  if (!emf.InitFromData(emf_data.data(), emf_data.length()))
    return nullptr;

  auto meta_header = std::make_unique<ENHMETAHEADER>();
  size_t header_size = sizeof(ENHMETAHEADER);
  if (GetEnhMetaFileHeader(emf.emf(), header_size, meta_header.get()) !=
      header_size) {
    return nullptr;
  }
  return meta_header;
}

void CompareEMFHeaders(const ENHMETAHEADER& expected_header,
                       const ENHMETAHEADER& actual_header) {
  // TODO(crbug.com/781403): once the EMF generation is fixed, also compare:
  //  rclBounds, rclFrame, szlDevice, szlMillimeters and szlMicrometers.
  EXPECT_EQ(expected_header.iType, actual_header.iType);
  EXPECT_EQ(expected_header.nSize, actual_header.nSize);
  EXPECT_EQ(expected_header.dSignature, actual_header.dSignature);
  EXPECT_EQ(expected_header.nVersion, actual_header.nVersion);
  EXPECT_EQ(expected_header.nBytes, actual_header.nBytes);
  EXPECT_EQ(expected_header.nRecords, actual_header.nRecords);
  EXPECT_EQ(expected_header.nHandles, actual_header.nHandles);
  EXPECT_EQ(expected_header.sReserved, actual_header.sReserved);
  EXPECT_EQ(expected_header.nDescription, actual_header.nDescription);
  EXPECT_EQ(expected_header.offDescription, actual_header.offDescription);
  EXPECT_EQ(expected_header.nPalEntries, actual_header.nPalEntries);
  EXPECT_EQ(expected_header.cbPixelFormat, actual_header.cbPixelFormat);
  EXPECT_EQ(expected_header.offPixelFormat, actual_header.offPixelFormat);
  EXPECT_EQ(expected_header.bOpenGL, actual_header.bOpenGL);
}

}  // namespace

IN_PROC_BROWSER_TEST_F(PDFToEMFConverterBrowserTest, TestFailure) {
  scoped_refptr<base::RefCountedStaticMemory> bad_pdf_data =
      base::MakeRefCounted<base::RefCountedStaticMemory>("0123456789", 10);

  base::RunLoop run_loop;
  int page_count = -1;
  std::unique_ptr<PdfConverter> pdf_converter = PdfConverter::StartPdfConverter(
      bad_pdf_data, PdfRenderSettings(),
      base::Bind(&StartCallbackImpl, run_loop.QuitClosure(), &page_count));
  run_loop.Run();
  EXPECT_EQ(0, page_count);
}

IN_PROC_BROWSER_TEST_F(PDFToEMFConverterBrowserTest, TestSuccess) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  base::FilePath test_data_dir;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir));
  test_data_dir = test_data_dir.AppendASCII("printing");
  base::FilePath pdf_file = test_data_dir.AppendASCII("pdf_to_emf_test.pdf");
  std::string pdf_data_str;
  ASSERT_TRUE(base::ReadFileToString(pdf_file, &pdf_data_str));
  ASSERT_GT(pdf_data_str.length(), 0U);
  scoped_refptr<base::RefCountedString> pdf_data(
      base::RefCountedString::TakeString(&pdf_data_str));

  // A4 page format.
  PdfRenderSettings pdf_settings(gfx::Rect(0, 0, 1700, 2200), gfx::Point(0, 0),
                                 /*dpi=*/gfx::Size(200, 200),
                                 /*autorotate=*/false,
                                 PdfRenderSettings::Mode::NORMAL);

  constexpr int kNumberOfPages = 3;
  std::unique_ptr<PdfConverter> pdf_converter;
  {
    base::RunLoop run_loop;
    int page_count = -1;
    pdf_converter = PdfConverter::StartPdfConverter(
        pdf_data, pdf_settings,
        base::Bind(&StartCallbackImpl, run_loop.QuitClosure(), &page_count));
    run_loop.Run();
    ASSERT_EQ(kNumberOfPages, page_count);
  }

  for (int i = 0; i < kNumberOfPages; i++) {
    int page_number = -1;
    std::unique_ptr<MetafilePlayer> emf_file;
    base::RunLoop run_loop;
    pdf_converter->GetPage(
        i, base::Bind(&GetPageCallbackImpl, run_loop.QuitClosure(),
                      &page_number, &emf_file));
    run_loop.Run();
    ASSERT_EQ(i, page_number);

    // We have to save the EMF to a file as LazyEmf::GetDataAsVector() is not
    // implemented.
    base::FilePath actual_emf_file_path;
    ASSERT_TRUE(base::CreateTemporaryFile(&actual_emf_file_path));
    base::File actual_emf_file(
        actual_emf_file_path,
        base::File::Flags::FLAG_CREATE_ALWAYS | base::File::Flags::FLAG_WRITE);
    ASSERT_TRUE(emf_file->SaveTo(&actual_emf_file));
    actual_emf_file.Close();
    std::string actual_emf_data;
    ASSERT_TRUE(base::ReadFileToString(actual_emf_file_path, &actual_emf_data));
    ASSERT_GT(actual_emf_data.length(), 0U);

    base::FilePath expected_emf_file = test_data_dir.AppendASCII(
        "pdf_to_emf_test_page_" +
        std::to_string(i + 1 /* page numbers start at 1 */) + ".emf");
    std::string expected_emf_data;
    ASSERT_TRUE(base::ReadFileToString(expected_emf_file, &expected_emf_data));
    ASSERT_GT(expected_emf_data.length(), 0U);

    // TODO(crbug.com/781403): the generated data can differ visually. Until
    // this is fixed only checking the output size and parts of the EMF header.
    ASSERT_EQ(expected_emf_data.length(), actual_emf_data.length());

    std::unique_ptr<ENHMETAHEADER> expected_header =
        GetEMFHeader(expected_emf_data);
    ASSERT_TRUE(expected_header);
    std::unique_ptr<ENHMETAHEADER> actual_header =
        GetEMFHeader(actual_emf_data);
    ASSERT_TRUE(actual_header);
    CompareEMFHeaders(*expected_header, *actual_header);
  }
}

}  // namespace printing
