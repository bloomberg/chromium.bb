// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/display_manager_test_api.h"

#include <vector>

#include "ash/display/display_manager.h"
#include "ash/shell.h"
#include "base/string_split.h"
#include "ui/aura/root_window.h"
#include "ui/gfx/display.h"

namespace ash {
namespace test {
namespace {

std::vector<gfx::Display> CreateDisplaysFromString(
    const std::string specs) {
  std::vector<gfx::Display> displays;
  std::vector<std::string> parts;
  base::SplitString(specs, ',', &parts);
  for (std::vector<std::string>::const_iterator iter = parts.begin();
       iter != parts.end(); ++iter) {
    displays.push_back(internal::CreateDisplayFromSpec(*iter));
  }
  return displays;
}

}  // namespace

DisplayManagerTestApi::DisplayManagerTestApi(
    internal::DisplayManager* display_manager)
        : display_manager_(display_manager) {
}

DisplayManagerTestApi::~DisplayManagerTestApi() {}

void DisplayManagerTestApi::UpdateDisplay(
    const std::string& display_specs) {
  std::vector<gfx::Display> displays = CreateDisplaysFromString(display_specs);
  bool is_host_origin_set = false;
  for (size_t i = 0; i < displays.size(); ++i) {
    if (displays[i].bounds_in_pixel().origin() != gfx::Point(0, 0)) {
      is_host_origin_set = true;
      break;
    }
  }

  // On non-testing environment, when a secondary display is connected, a new
  // native (i.e. X) window for the display is always created below the
  // previous one for GPU performance reasons. Try to emulate the behavior
  // unless host origins are explicitly set.
  if (!is_host_origin_set) {
    // Sart from (1,1) so that windows won't overlap with native mouse cursor.
    // See |AshTestBase::SetUp()|.
    int next_y = 1;
    for (std::vector<gfx::Display>::iterator iter = displays.begin();
         iter != displays.end(); ++iter) {
      gfx::Rect bounds(iter->GetSizeInPixel());
      bounds.set_x(1);
      bounds.set_y(next_y);
      next_y += bounds.height();
      iter->SetScaleAndBounds(iter->device_scale_factor(), bounds);
    }
  }

  display_manager_->SetDisplayIdsForTest(&displays);
  display_manager_->OnNativeDisplaysChanged(displays);
}

}  // namespace test
}  // namespace ash
