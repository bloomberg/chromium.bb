// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/in_process_browser_test.h"

#include "base/files/file_util.h"
#include "base/memory/ref_counted_memory.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/printing/pwg_raster_converter.h"
#include "printing/pdf_render_settings.h"
#include "printing/pwg_raster_settings.h"

namespace printing {

namespace {

void ResultCallbackImpl(bool* called,
                        bool* success_out,
                        base::FilePath* temp_file_out,
                        base::Closure quit_closure,
                        bool success_in,
                        const base::FilePath& temp_file_in) {
  *called = true;
  *success_out = success_in;
  *temp_file_out = temp_file_in;
  quit_closure.Run();
}

class PDFToPWGRasterBrowserTest : public InProcessBrowserTest {
 public:
  PDFToPWGRasterBrowserTest()
      : converter_(PWGRasterConverter::CreateDefault()) {}
  ~PDFToPWGRasterBrowserTest() override {}

  void Convert(base::RefCountedMemory* pdf_data,
               const PdfRenderSettings& conversion_settings,
               const PwgRasterSettings& bitmap_settings,
               bool expect_success,
               base::FilePath* temp_file) {
    bool called = false;
    bool success = false;
    base::RunLoop run_loop;
    converter_->Start(pdf_data, conversion_settings, bitmap_settings,
                      base::Bind(&ResultCallbackImpl, &called, &success,
                                 temp_file, run_loop.QuitClosure()));
    run_loop.Run();
    ASSERT_TRUE(called);
    EXPECT_EQ(success, expect_success);
  }

 private:
  std::unique_ptr<PWGRasterConverter> converter_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(PDFToPWGRasterBrowserTest, TestFailure) {
  scoped_refptr<base::RefCountedStaticMemory> bad_pdf_data =
      base::MakeRefCounted<base::RefCountedStaticMemory>("0123456789", 10);
  base::FilePath temp_file;
  Convert(bad_pdf_data.get(), PdfRenderSettings(), PwgRasterSettings(),
          /*expect_success=*/false, &temp_file);
}

IN_PROC_BROWSER_TEST_F(PDFToPWGRasterBrowserTest, TestSuccess) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  base::FilePath test_data_dir;
  ASSERT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &test_data_dir));
  base::FilePath pdf_file = test_data_dir.AppendASCII(
      "chrome/test/data/printing/pdf_to_pwg_raster_test.pdf");
  std::string pdf_data_str;
  ASSERT_TRUE(base::ReadFileToString(pdf_file, &pdf_data_str));
  ASSERT_GT(pdf_data_str.length(), 0U);
  scoped_refptr<base::RefCountedString> pdf_data(
      base::RefCountedString::TakeString(&pdf_data_str));

  PdfRenderSettings pdf_settings(gfx::Rect(0, 0, 500, 500), gfx::Point(0, 0),
                                 /*dpi=*/1000, /*autorotate=*/false,
                                 PdfRenderSettings::Mode::NORMAL);
  PwgRasterSettings pwg_settings;
  pwg_settings.odd_page_transform = PwgRasterTransformType::TRANSFORM_NORMAL;
  pwg_settings.rotate_all_pages = false;
  pwg_settings.reverse_page_order = false;

  base::FilePath temp_file;
  Convert(pdf_data.get(), pdf_settings, pwg_settings,
          /*expect_success=*/true, &temp_file);
  ASSERT_FALSE(temp_file.empty());

  // Note that for some reason the generated PWG varies depending on the
  // platform (32 or 64 bits) on Linux.
  base::FilePath pwg_file = test_data_dir.AppendASCII(
#if defined(OS_LINUX) && defined(ARCH_CPU_32_BITS)
      "chrome/test/data/printing/pdf_to_pwg_raster_test_32.pwg");
#else
      "chrome/test/data/printing/pdf_to_pwg_raster_test.pwg");
#endif

  std::string pwg_expected_data_str;
  ASSERT_TRUE(base::ReadFileToString(pwg_file, &pwg_expected_data_str));
  std::string pwg_actual_data_str;
  ASSERT_TRUE(base::ReadFileToString(temp_file, &pwg_actual_data_str));
  ASSERT_EQ(pwg_expected_data_str, pwg_actual_data_str);
}

}  // namespace printing
