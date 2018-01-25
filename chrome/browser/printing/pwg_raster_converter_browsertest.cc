// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/in_process_browser_test.h"

#include "base/files/file_util.h"
#include "base/memory/ref_counted_memory.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/sha1.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/printing/pwg_raster_converter.h"
#include "chrome/common/chrome_paths.h"
#include "printing/pdf_render_settings.h"
#include "printing/pwg_raster_settings.h"

namespace printing {

namespace {

// Note that for some reason the generated PWG varies depending on the
// platform (32 or 64 bits) on Linux.
#if defined(OS_LINUX) && defined(ARCH_CPU_32_BITS)
constexpr char kPdfToPwgRasterColorTestFile[] = "pdf_to_pwg_raster_test_32.pwg";
constexpr char kPdfToPwgRasterMonoTestFile[] =
    "pdf_to_pwg_raster_mono_test_32.pwg";
#else
constexpr char kPdfToPwgRasterColorTestFile[] = "pdf_to_pwg_raster_test.pwg";
constexpr char kPdfToPwgRasterMonoTestFile[] =
    "pdf_to_pwg_raster_mono_test.pwg";
#endif

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

void GetPdfData(const char* file_name,
                base::FilePath* test_data_dir,
                scoped_refptr<base::RefCountedString>* pdf_data) {
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, test_data_dir));
  *test_data_dir = test_data_dir->AppendASCII("printing");
  base::FilePath pdf_file = test_data_dir->AppendASCII(file_name);
  std::string pdf_data_str;
  ASSERT_TRUE(base::ReadFileToString(pdf_file, &pdf_data_str));
  ASSERT_GT(pdf_data_str.length(), 0U);
  *pdf_data = base::RefCountedString::TakeString(&pdf_data_str);
}

std::string HashFile(const std::string& file_data) {
  std::string sha1 = base::SHA1HashString(file_data);
  return base::HexEncode(sha1.c_str(), sha1.length());
}

void ComparePwgFiles(const base::FilePath& expected,
                     const base::FilePath& actual) {
  std::string pwg_expected_data_str;
  ASSERT_TRUE(base::ReadFileToString(expected, &pwg_expected_data_str));
  std::string pwg_actual_data_str;
  ASSERT_TRUE(base::ReadFileToString(actual, &pwg_actual_data_str));
  EXPECT_EQ(pwg_expected_data_str.length(), pwg_actual_data_str.length());
  EXPECT_EQ(HashFile(pwg_expected_data_str), HashFile(pwg_actual_data_str));
}

class PdfToPwgRasterBrowserTest : public InProcessBrowserTest {
 public:
  PdfToPwgRasterBrowserTest()
      : converter_(PwgRasterConverter::CreateDefault()) {}
  ~PdfToPwgRasterBrowserTest() override {}

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
  std::unique_ptr<PwgRasterConverter> converter_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(PdfToPwgRasterBrowserTest, TestFailure) {
  scoped_refptr<base::RefCountedStaticMemory> bad_pdf_data =
      base::MakeRefCounted<base::RefCountedStaticMemory>("0123456789", 10);
  base::FilePath temp_file;
  Convert(bad_pdf_data.get(), PdfRenderSettings(), PwgRasterSettings(),
          /*expect_success=*/false, &temp_file);
}

IN_PROC_BROWSER_TEST_F(PdfToPwgRasterBrowserTest, TestSuccessColor) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  base::FilePath test_data_dir;
  scoped_refptr<base::RefCountedString> pdf_data;
  GetPdfData("pdf_to_pwg_raster_test.pdf", &test_data_dir, &pdf_data);

  PdfRenderSettings pdf_settings(gfx::Rect(0, 0, 500, 500), gfx::Point(0, 0),
                                 /*dpi=*/gfx::Size(1000, 1000),
                                 /*autorotate=*/false,
                                 PdfRenderSettings::Mode::NORMAL);
  PwgRasterSettings pwg_settings;
  pwg_settings.odd_page_transform = PwgRasterTransformType::TRANSFORM_NORMAL;
  pwg_settings.rotate_all_pages = false;
  pwg_settings.reverse_page_order = false;
  pwg_settings.use_color = true;

  base::FilePath temp_file;
  Convert(pdf_data.get(), pdf_settings, pwg_settings,
          /*expect_success=*/true, &temp_file);
  ASSERT_FALSE(temp_file.empty());

  base::FilePath pwg_file =
      test_data_dir.AppendASCII(kPdfToPwgRasterColorTestFile);
  ComparePwgFiles(pwg_file, temp_file);
}

IN_PROC_BROWSER_TEST_F(PdfToPwgRasterBrowserTest, TestSuccessMono) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  base::FilePath test_data_dir;
  scoped_refptr<base::RefCountedString> pdf_data;
  GetPdfData("pdf_to_pwg_raster_test.pdf", &test_data_dir, &pdf_data);

  PdfRenderSettings pdf_settings(gfx::Rect(0, 0, 500, 500), gfx::Point(0, 0),
                                 /*dpi=*/gfx::Size(1000, 1000),
                                 /*autorotate=*/false,
                                 PdfRenderSettings::Mode::NORMAL);
  PwgRasterSettings pwg_settings;
  pwg_settings.odd_page_transform = PwgRasterTransformType::TRANSFORM_NORMAL;
  pwg_settings.rotate_all_pages = false;
  pwg_settings.reverse_page_order = false;
  pwg_settings.use_color = false;

  base::FilePath temp_file;
  Convert(pdf_data.get(), pdf_settings, pwg_settings,
          /*expect_success=*/true, &temp_file);
  ASSERT_FALSE(temp_file.empty());

  base::FilePath pwg_file =
      test_data_dir.AppendASCII(kPdfToPwgRasterMonoTestFile);
  ComparePwgFiles(pwg_file, temp_file);
}

}  // namespace printing
