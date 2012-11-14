// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/display_manager_test_api.h"

#include <vector>

#include "ash/display/display_manager.h"
#include "ash/shell.h"
#include "base/string_split.h"
#include "ui/aura/display_util.h"
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
    displays.push_back(aura::CreateDisplayFromSpec(*iter));
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
  display_manager_->SetDisplayIdsForTest(&displays);
  display_manager_->OnNativeDisplaysChanged(displays);

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
    Shell::RootWindowList root_windows = Shell::GetAllRootWindows();
    int next_y = 0;
    for (size_t i = 0; i < root_windows.size(); ++i) {
      const gfx::Size size = root_windows[i]->GetHostSize();
      root_windows[i]->SetHostBounds(gfx::Rect(gfx::Point(0, next_y), size));
      next_y += size.height();
    }
  }
}

}  // namespace test
}  // namespace ash
