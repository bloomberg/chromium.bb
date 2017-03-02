// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/wallpaper/wallpaper_color_calculator.h"

#include <memory>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/test/null_task_runner.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/wallpaper/wallpaper_color_calculator_observer.h"
#include "skia/ext/platform_canvas.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_analysis.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"

namespace wallpaper {
namespace {

const SkColor kDefaultColor = SK_ColorTRANSPARENT;

class TestWallpaperColorCalculatorObserver
    : public WallpaperColorCalculatorObserver {
 public:
  TestWallpaperColorCalculatorObserver() {}

  ~TestWallpaperColorCalculatorObserver() override {}

  bool WasNotified() const { return notified_; }

  // WallpaperColorCalculatorObserver:
  void OnColorCalculationComplete() override { notified_ = true; }

 private:
  bool notified_ = false;

  DISALLOW_COPY_AND_ASSIGN(TestWallpaperColorCalculatorObserver);
};

}  // namespace

class WallPaperColorCalculatorTest : public testing::Test {
 public:
  WallPaperColorCalculatorTest();
  ~WallPaperColorCalculatorTest() override;

 protected:
  void InstallTaskRunner(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  gfx::ImageSkia image_;

  std::unique_ptr<WallpaperColorCalculator> calculator_;

  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;

  TestWallpaperColorCalculatorObserver observer_;

 private:
  // Required by PostTaskAndReplyImpl.
  std::unique_ptr<base::ThreadTaskRunnerHandle> task_runner_handle_;

  DISALLOW_COPY_AND_ASSIGN(WallPaperColorCalculatorTest);
};

WallPaperColorCalculatorTest::WallPaperColorCalculatorTest()
    : task_runner_(new base::TestMockTimeTaskRunner()) {
  // Creates an |image_| that will yield a non-default prominent color.
  const gfx::Size kImageSize(300, 200);
  const SkColor kGray = SkColorSetRGB(10, 10, 10);
  const SkColor kVibrantGreen = SkColorSetRGB(25, 200, 25);

  gfx::Canvas canvas(kImageSize, 1.0f, true);
  canvas.FillRect(gfx::Rect(kImageSize), kGray);
  canvas.FillRect(gfx::Rect(0, 1, 300, 1), kVibrantGreen);

  image_ = gfx::ImageSkia::CreateFrom1xBitmap(canvas.ToBitmap());

  calculator_ = base::MakeUnique<WallpaperColorCalculator>(
      image_, color_utils::LumaRange::NORMAL,
      color_utils::SaturationRange::VIBRANT, nullptr);
  calculator_->AddObserver(&observer_);

  InstallTaskRunner(task_runner_);
}

WallPaperColorCalculatorTest::~WallPaperColorCalculatorTest() {}

void WallPaperColorCalculatorTest::InstallTaskRunner(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  task_runner_handle_.reset();
  task_runner_handle_ =
      base::MakeUnique<base::ThreadTaskRunnerHandle>(task_runner);
  calculator_->SetTaskRunnerForTest(task_runner);
}

TEST_F(WallPaperColorCalculatorTest,
       StartCalculationReturnsFalseWhenPostingTaskFails) {
  scoped_refptr<base::NullTaskRunner> task_runner = new base::NullTaskRunner();
  InstallTaskRunner(task_runner);
  calculator_->set_prominent_color_for_test(SK_ColorBLACK);

  EXPECT_FALSE(calculator_->StartCalculation());
  EXPECT_EQ(kDefaultColor, calculator_->prominent_color());
}

TEST_F(WallPaperColorCalculatorTest, ObserverNotifiedOnSuccessfulCalculation) {
  EXPECT_FALSE(observer_.WasNotified());

  EXPECT_TRUE(calculator_->StartCalculation());
  EXPECT_FALSE(observer_.WasNotified());

  task_runner_->RunUntilIdle();
  EXPECT_TRUE(observer_.WasNotified());
}

TEST_F(WallPaperColorCalculatorTest, ColorUpdatedOnSuccessfulCalculation) {
  calculator_->set_prominent_color_for_test(kDefaultColor);

  EXPECT_TRUE(calculator_->StartCalculation());
  EXPECT_EQ(kDefaultColor, calculator_->prominent_color());

  task_runner_->RunUntilIdle();
  EXPECT_NE(kDefaultColor, calculator_->prominent_color());
}

TEST_F(WallPaperColorCalculatorTest,
       NoCrashWhenCalculatorDestroyedBeforeTaskProcessing) {
  EXPECT_TRUE(calculator_->StartCalculation());
  calculator_.reset();

  EXPECT_TRUE(task_runner_->HasPendingTask());

  task_runner_->RunUntilIdle();
  EXPECT_FALSE(observer_.WasNotified());
  EXPECT_FALSE(task_runner_->HasPendingTask());
}

}  // namespace wallpaper
