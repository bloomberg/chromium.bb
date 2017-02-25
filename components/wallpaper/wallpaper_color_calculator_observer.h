// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WALLPAPER_WALLPAPER_COLOR_CALCULATOR_OBSERVER_H_
#define COMPONENTS_WALLPAPER_WALLPAPER_COLOR_CALCULATOR_OBSERVER_H_

#include "components/wallpaper/wallpaper_export.h"

namespace wallpaper {

// Observer for the WallpaperColorCalculator.
class WALLPAPER_EXPORT WallpaperColorCalculatorObserver {
 public:
  // Notified when a color calculation completes.
  virtual void OnColorCalculationComplete() = 0;

 protected:
  virtual ~WallpaperColorCalculatorObserver() {}
};

}  // namespace wallpaper

#endif  // COMPONENTS_WALLPAPER_WALLPAPER_COLOR_CALCULATOR_OBSERVER_H_
