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

namespace wallpaper {

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
  start_calculation_time_ = base::Time::Now();

  image_.MakeThreadSafe();
  if (base::PostTaskAndReplyWithResult(
          task_runner_.get(), FROM_HERE,
          base::Bind(&color_utils::CalculateProminentColorOfBitmap,
                     *image_.bitmap(), luma_, saturation_),
          base::Bind(&WallpaperColorCalculator::NotifyCalculationComplete,
                     weak_ptr_factory_.GetWeakPtr()))) {
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

void WallpaperColorCalculator::NotifyCalculationComplete(
    SkColor prominent_color) {
  UMA_HISTOGRAM_MEDIUM_TIMES("Ash.Wallpaper.TimeSpentExtractingColors",
                             base::Time::Now() - start_calculation_time_);

  prominent_color_ = prominent_color;
  for (auto& observer : observers_)
    observer.OnColorCalculationComplete();

  // This could be deleted!
}

}  // namespace wallpaper
