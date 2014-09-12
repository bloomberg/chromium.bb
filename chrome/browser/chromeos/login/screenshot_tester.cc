// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screenshot_tester.h"

#include "ash/shell.h"
#include "base/base_export.h"
#include "base/bind_internal.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/prefs/pref_service.h"
#include "base/run_loop.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/pref_names.h"
#include "chromeos/chromeos_switches.h"
#include "content/public/browser/browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image.h"
#include "ui/snapshot/snapshot.h"

namespace {

// Sets test mode for screenshot testing.
const char kTestMode[] = "test";

// Sets update mode for screenshot testing.
const char kUpdateMode[] = "update";

}  // namespace

namespace chromeos {

ScreenshotTester::ScreenshotTester() : test_mode_(false), weak_factory_(this) {
}

ScreenshotTester::~ScreenshotTester() {
}

bool ScreenshotTester::TryInitialize() {
  CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (!command_line.HasSwitch(switches::kEnableScreenshotTestingWithMode))
    return false;
  if (!command_line.HasSwitch(::switches::kEnablePixelOutputInTests) ||
      !command_line.HasSwitch(::switches::kUIEnableImplSidePainting)) {
    // TODO(elizavetai): make turning on --enable-pixel-output-in-tests
    // and --ui-enable-impl-side-painting automatical.
    LOG(ERROR) << "--enable-pixel-output-in-tests and "
               << "--ui-enable-impl-side-painting are required to take "
               << "screenshots";
    return false;
  }

  std::string mode = command_line.GetSwitchValueASCII(
      switches::kEnableScreenshotTestingWithMode);
  if (mode != kUpdateMode && mode != kTestMode) {
    CHECK(false) << "Invalid mode for screenshot testing: " << mode;
  }

  if (!command_line.HasSwitch(chromeos::switches::kGoldenScreenshotsDir)) {
    CHECK(false) << "No directory for golden screenshots specified";
  }

  golden_screenshots_dir_ =
      command_line.GetSwitchValuePath(switches::kGoldenScreenshotsDir);

  if (mode == kTestMode) {
    test_mode_ = true;
    if (!command_line.HasSwitch(switches::kArtifactsDir)) {
      artifacts_dir_ = golden_screenshots_dir_;
      LOG(WARNING)
          << "No directory for artifact storing specified. Artifacts will be"
          << "saved at golden screenshots directory.";
    } else {
      artifacts_dir_ = command_line.GetSwitchValuePath(switches::kArtifactsDir);
    }
  }
  return true;
}

void ScreenshotTester::Run(const std::string& test_name) {
  test_name_ = test_name;
  PNGFile current_screenshot = TakeScreenshot();
  if (test_mode_) {
    PNGFile golden_screenshot = LoadGoldenScreenshot();
    VLOG(0) << "Loaded golden screenshot";
    CompareScreenshots(golden_screenshot, current_screenshot);
  } else {
    UpdateGoldenScreenshot(current_screenshot);
  }
}

void ScreenshotTester::UpdateGoldenScreenshot(PNGFile png_data) {
  CHECK(SaveImage("golden_screenshot", golden_screenshots_dir_, png_data));
}

bool ScreenshotTester::SaveImage(const std::string& file_name,
                                 const base::FilePath& screenshot_dir,
                                 PNGFile png_data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::FilePath screenshot_path =
      screenshot_dir.AppendASCII(test_name_ + "_" + file_name + ".png");
  if (!png_data.get()) {
    LOG(ERROR) << "Can't take a screenshot";
    return false;
  }
  if (!base::CreateDirectory(screenshot_path.DirName())) {
    LOG(ERROR) << "Can't create a directory "
               << screenshot_path.DirName().value();
    return false;
  }
  if (static_cast<size_t>(
          base::WriteFile(screenshot_path,
                          reinterpret_cast<char*>(&(png_data->data()[0])),
                          png_data->size())) != png_data->size()) {
    LOG(ERROR) << "Can't save screenshot " << file_name;
    return false;
  }
  VLOG(0) << "Screenshot " << file_name << ".png saved successfully to "
          << screenshot_dir.value();
  return true;
}

void ScreenshotTester::ReturnScreenshot(const PNGFile& screenshot,
                                        PNGFile png_data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  screenshot->data() = png_data->data();
  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE, run_loop_quitter_);
}

ScreenshotTester::PNGFile ScreenshotTester::TakeScreenshot() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  aura::Window* primary_window = ash::Shell::GetPrimaryRootWindow();
  gfx::Rect rect = primary_window->bounds();
  PNGFile screenshot = new base::RefCountedBytes;
  ui::GrabWindowSnapshotAsync(primary_window,
                              rect,
                              content::BrowserThread::GetBlockingPool(),
                              base::Bind(&ScreenshotTester::ReturnScreenshot,
                                         weak_factory_.GetWeakPtr(),
                                         screenshot));
  base::RunLoop run_loop;
  run_loop_quitter_ = run_loop.QuitClosure();
  run_loop.Run();
  return screenshot;
}

ScreenshotTester::PNGFile ScreenshotTester::LoadGoldenScreenshot() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::FilePath screenshot_path = golden_screenshots_dir_.AppendASCII(
      test_name_ + "_golden_screenshot.png");
  if (!base::PathExists(screenshot_path)) {
    CHECK(false) << "Can't find a golden screenshot for this test";
  }

  int64 golden_screenshot_size;
  base::GetFileSize(screenshot_path, &golden_screenshot_size);

  if (golden_screenshot_size == -1) {
    CHECK(false) << "Can't get golden screenshot size";
  }
  PNGFile png_data = new base::RefCountedBytes;
  png_data->data().resize(golden_screenshot_size);
  base::ReadFile(screenshot_path,
                 reinterpret_cast<char*>(&(png_data->data()[0])),
                 golden_screenshot_size);

  return png_data;
}

void ScreenshotTester::CompareScreenshots(PNGFile model, PNGFile sample) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  ASSERT_TRUE(model.get());
  ASSERT_TRUE(sample.get());

  SkBitmap model_bitmap;
  SkBitmap sample_bitmap;
  gfx::PNGCodec::Decode(reinterpret_cast<unsigned char*>(&(model->data()[0])),
                        model->data().size(),
                        &model_bitmap);
  gfx::PNGCodec::Decode(reinterpret_cast<unsigned char*>(&(sample->data()[0])),
                        sample->data().size(),
                        &sample_bitmap);

  ASSERT_EQ(model_bitmap.width(), sample_bitmap.width());
  ASSERT_EQ(model_bitmap.height(), sample_bitmap.height());

  bool screenshots_match = true;

  SkCanvas diff_canvas(sample_bitmap);
  for (int i = 0; i < model_bitmap.width(); i++)
    for (int j = 0; j < model_bitmap.height(); j++) {
      if (model_bitmap.getColor(i, j) == sample_bitmap.getColor(i, j)) {
        diff_canvas.drawPoint(i, j, SK_ColorWHITE);
      } else {
        screenshots_match = false;
        diff_canvas.drawPoint(i, j, SK_ColorRED);
      }
    }

  if (!screenshots_match) {
    CHECK(SaveImage("failed_screenshot", artifacts_dir_, sample));
    gfx::PNGCodec::EncodeBGRASkBitmap(sample_bitmap, false, &sample->data());
    CHECK(SaveImage("difference", artifacts_dir_, sample));
    LOG(ERROR)
        << "Screenshots testing failed. Screenshots differ in some pixels";
    VLOG(0) << "Current screenshot and diff picture saved to "
            << artifacts_dir_.value();
  } else {
    VLOG(0) << "Current screenshot matches the golden screenshot. Screenshot "
               "testing passed.";
  }
  ASSERT_TRUE(screenshots_match);
}

}  // namespace chromeos
