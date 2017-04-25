// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/wallpaper/wallpaper_color_calculator.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/task_runner.h"
#include "base/task_runner_util.h"
#include "components/wallpaper/wallpaper_color_calculator_observer.h"

namespace wallpaper {

namespace {

// The largest image size, in pixels, to synchronously calculate the prominent
// color. This is a simple heuristic optimization because extraction on images
// smaller than this should run very quickly, and offloading the task to another
// thread would actually take longer.
const int kMaxPixelsForSynchronousCalculation = 100;

// Wrapper for color_utils::CalculateProminentColorOfBitmap() that records
// wallpaper specific metrics.
//
// NOTE: |image| is intentionally a copy to ensure it exists for the duration of
// the calculation.
SkColor CalculateWallpaperColor(const gfx::ImageSkia image,
                                color_utils::LumaRange luma,
                                color_utils::SaturationRange saturation) {
  base::TimeTicks start_time = base::TimeTicks::Now();
  const SkColor prominent_color = color_utils::CalculateProminentColorOfBitmap(
      *image.bitmap(), luma, saturation);

  UMA_HISTOGRAM_TIMES("Ash.Wallpaper.ColorExtraction.Durations",
                      base::TimeTicks::Now() - start_time);
  UMA_HISTOGRAM_BOOLEAN("Ash.Wallpaper.ColorExtractionResult",
                        prominent_color != SK_ColorTRANSPARENT);

  return prominent_color;
}

bool ShouldCalculateSync(const gfx::ImageSkia& image) {
  return image.width() * image.height() <= kMaxPixelsForSynchronousCalculation;
}

}  // namespace

WallpaperColorCalculator::WallpaperColorCalculator(
    const gfx::ImageSkia& image,
    color_utils::LumaRange luma,
    color_utils::SaturationRange saturation,
    scoped_refptr<base::TaskRunner> task_runner)
    : image_(image),
      luma_(luma),
      saturation_(saturation),
      task_runner_(std::move(task_runner)),
      weak_ptr_factory_(this) {}

WallpaperColorCalculator::~WallpaperColorCalculator() {}

void WallpaperColorCalculator::AddObserver(
    WallpaperColorCalculatorObserver* observer) {
  observers_.AddObserver(observer);
}

void WallpaperColorCalculator::RemoveObserver(
    WallpaperColorCalculatorObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool WallpaperColorCalculator::StartCalculation() {
  if (ShouldCalculateSync(image_)) {
    const SkColor prominent_color =
        CalculateWallpaperColor(image_, luma_, saturation_);
    NotifyCalculationComplete(prominent_color);
    return true;
  }

  image_.MakeThreadSafe();
  if (base::PostTaskAndReplyWithResult(
          task_runner_.get(), FROM_HERE,
          base::Bind(&CalculateWallpaperColor, image_, luma_, saturation_),
          base::Bind(&WallpaperColorCalculator::OnAsyncCalculationComplete,
                     weak_ptr_factory_.GetWeakPtr(), base::TimeTicks::Now()))) {
    return true;
  }

  LOG(WARNING) << "PostSequencedWorkerTask failed. "
               << "Wallpaper promiment color may not be calculated.";

  prominent_color_ = SK_ColorTRANSPARENT;
  return false;
}

void WallpaperColorCalculator::SetTaskRunnerForTest(
    scoped_refptr<base::TaskRunner> task_runner) {
  task_runner_ = task_runner;
}

void WallpaperColorCalculator::OnAsyncCalculationComplete(
    base::TimeTicks async_start_time,
    SkColor prominent_color) {
  UMA_HISTOGRAM_TIMES("Ash.Wallpaper.ColorExtraction.UserDelay",
                      base::TimeTicks::Now() - async_start_time);
  NotifyCalculationComplete(prominent_color);
}

void WallpaperColorCalculator::NotifyCalculationComplete(
    SkColor prominent_color) {
  prominent_color_ = prominent_color;
  for (auto& observer : observers_)
    observer.OnColorCalculationComplete();

  // This could be deleted!
}

}  // namespace wallpaper
