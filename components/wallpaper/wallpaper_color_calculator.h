// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WALLPAPER_WALLPAPER_COLOR_CALCULATOR_H_
#define COMPONENTS_WALLPAPER_WALLPAPER_COLOR_CALCULATOR_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "components/wallpaper/wallpaper_color_calculator_observer.h"
#include "components/wallpaper/wallpaper_export.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/color_analysis.h"
#include "ui/gfx/image/image_skia.h"

namespace base {
class TaskRunner;
}

namespace wallpaper {
class WallpaperColorCalculatorObserver;

// Asynchronously calculates colors based on a wallpaper image.
class WALLPAPER_EXPORT WallpaperColorCalculator {
 public:
  // |image|, |luma| and |saturation| are the input parameters to the color
  // calculation that is executed on the |task_runner|.
  WallpaperColorCalculator(const gfx::ImageSkia& image,
                           color_utils::LumaRange luma,
                           color_utils::SaturationRange saturation,
                           scoped_refptr<base::TaskRunner> task_runner);
  ~WallpaperColorCalculator();

  void AddObserver(WallpaperColorCalculatorObserver* observer);

  void RemoveObserver(WallpaperColorCalculatorObserver* observer);

  // Initiates the calculation and returns false if the calculation fails to be
  // initiated.  Callers should be aware that this will make |image_| read-only.
  bool StartCalculation() WARN_UNUSED_RESULT;

  SkColor prominent_color() const { return prominent_color_; }

  void set_prominent_color_for_test(SkColor prominent_color) {
    prominent_color_ = prominent_color;
  }

  // Explicitly sets the |task_runner_| for testing.
  void SetTaskRunnerForTest(scoped_refptr<base::TaskRunner> task_runner);

 private:
  // Notifies observers that a color calulation has completed. Called on the
  // same thread that constructed |this|.
  void NotifyCalculationComplete(SkColor prominent_color);

  // The result of the color calculation.
  SkColor prominent_color_ = SK_ColorTRANSPARENT;

  // The image to calculate colors from.
  gfx::ImageSkia image_;

  // Input for the color calculation.
  color_utils::LumaRange luma_;

  // Input for the color calculation.
  color_utils::SaturationRange saturation_;

  // The task runner to run the calculation on.
  scoped_refptr<base::TaskRunner> task_runner_;

  // The time that StartCalculation() was last called. Used for recording
  // timing metrics.
  base::Time start_calculation_time_;

  base::ObserverList<WallpaperColorCalculatorObserver> observers_;

  base::WeakPtrFactory<WallpaperColorCalculator> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(WallpaperColorCalculator);
};

}  // namespace wallpaper

#endif  // COMPONENTS_WALLPAPER_WALLPAPER_COLOR_CALCULATOR_H_
