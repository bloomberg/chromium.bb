// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/dip_px_util.h"

#include <cmath>

#include "base/numerics/safe_conversions.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

namespace {

float GetPrimaryDisplayScaleFactor() {
  display::Screen* screen = display::Screen::GetScreen();
  if (!screen) {
    return 1.0f;
  }
  return screen->GetPrimaryDisplay().device_scale_factor();
}

}  // namespace

namespace apps {

int ConvertDipToPx(int dip) {
  return base::saturated_cast<int>(
      std::floor(static_cast<float>(dip) * GetPrimaryDisplayScaleFactor()));
}

int ConvertPxToDip(int px) {
  return base::saturated_cast<int>(
      std::floor(static_cast<float>(px) / GetPrimaryDisplayScaleFactor()));
}

}  // namespace apps
