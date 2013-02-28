// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/display_manager_test_api.h"

#include <vector>

#include "ash/display/display_info.h"
#include "ash/display/display_manager.h"
#include "ash/shell.h"
#include "base/string_split.h"
#include "ui/aura/root_window.h"
#include "ui/gfx/display.h"

namespace ash {
namespace test {
typedef std::vector<gfx::Display> DisplayList;
typedef internal::DisplayInfo DisplayInfo;
typedef std::vector<DisplayInfo> DisplayInfoList;

namespace {

std::vector<DisplayInfo> CreateDisplayInfoListFromString(
    const std::string specs,
    internal::DisplayManager* display_manager) {
  std::vector<DisplayInfo> display_info_list;
  std::vector<std::string> parts;
  base::SplitString(specs, ',', &parts);
  int index = 0;
  for (std::vector<std::string>::const_iterator iter = parts.begin();
       iter != parts.end(); ++iter, ++index) {
    gfx::Display* display = display_manager->GetDisplayAt(index);
    int64 id = display ? display->id() : gfx::Display::kInvalidDisplayID;
    display_info_list.push_back(
        DisplayInfo::CreateFromSpecWithID(*iter, id));
  }
  return display_info_list;
}

}  // namespace

DisplayManagerTestApi::DisplayManagerTestApi(
    internal::DisplayManager* display_manager)
        : display_manager_(display_manager) {
}

DisplayManagerTestApi::~DisplayManagerTestApi() {}

void DisplayManagerTestApi::UpdateDisplay(
    const std::string& display_specs) {
  std::vector<DisplayInfo> display_info_list =
      CreateDisplayInfoListFromString(display_specs, display_manager_);
  bool is_host_origin_set = false;
  for (size_t i = 0; i < display_info_list.size(); ++i) {
    const DisplayInfo& display_info = display_info_list[i];
    if (display_info.bounds_in_pixel().origin() != gfx::Point(0, 0)) {
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
    for (std::vector<DisplayInfo>::iterator iter = display_info_list.begin();
         iter != display_info_list.end(); ++iter) {
      gfx::Rect bounds(iter->bounds_in_pixel().size());
      bounds.set_x(1);
      bounds.set_y(next_y);
      next_y += bounds.height();
      iter->SetBounds(bounds);
    }
  }

  display_manager_->OnNativeDisplaysChanged(display_info_list);
}

int64 DisplayManagerTestApi::SetFirstDisplayAsInternalDisplay() {
  const gfx::Display& internal = display_manager_->displays_[0];
  gfx::Display::SetInternalDisplayId(internal.id());
  display_manager_->internal_display_info_.reset(new DisplayInfo(
      display_manager_->GetDisplayInfo(internal)));
  return gfx::Display::InternalDisplayId();
}

}  // namespace test
}  // namespace ash
