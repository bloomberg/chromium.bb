// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/display_manager_test_api.h"

#include <vector>

#include "ash/ash_switches.h"
#include "ash/display/display_info.h"
#include "ash/display/display_layout_store.h"
#include "ash/display/display_manager.h"
#include "ash/display/display_util.h"
#include "ash/display/extended_mouse_warp_controller.h"
#include "ash/display/mouse_cursor_event_filter.h"
#include "ash/display/unified_mouse_warp_controller.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "base/command_line.h"
#include "base/strings/string_split.h"
#include "ui/aura/env.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/display.h"

namespace ash {
namespace test {
typedef std::vector<gfx::Display> DisplayList;
typedef DisplayInfo DisplayInfo;
typedef std::vector<DisplayInfo> DisplayInfoList;

namespace {

std::vector<DisplayInfo> CreateDisplayInfoListFromString(
    const std::string specs,
    DisplayManager* display_manager) {
  std::vector<DisplayInfo> display_info_list;
  std::vector<std::string> parts = base::SplitString(
      specs, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  size_t index = 0;

  DisplayManager::DisplayList list =
      display_manager->IsInUnifiedMode()
          ? display_manager->software_mirroring_display_list()
          : display_manager->active_display_list();

  for (std::vector<std::string>::const_iterator iter = parts.begin();
       iter != parts.end(); ++iter, ++index) {
    int64_t id = (index < list.size()) ? list[index].id()
                                       : gfx::Display::kInvalidDisplayID;
    display_info_list.push_back(
        DisplayInfo::CreateFromSpecWithID(*iter, id));
  }
  return display_info_list;
}

}  // namespace

// static
bool DisplayManagerTestApi::TestIfMouseWarpsAt(
    ui::test::EventGenerator& event_generator,
    const gfx::Point& point_in_screen) {
  DCHECK(!Shell::GetInstance()->display_manager()->IsInUnifiedMode());
  static_cast<ExtendedMouseWarpController*>(
      Shell::GetInstance()
          ->mouse_cursor_filter()
          ->mouse_warp_controller_for_test())
      ->allow_non_native_event_for_test();
  gfx::Screen* screen = gfx::Screen::GetScreen();
  gfx::Display original_display =
      screen->GetDisplayNearestPoint(point_in_screen);
  event_generator.MoveMouseTo(point_in_screen);
  return original_display.id() !=
         screen->GetDisplayNearestPoint(
                   aura::Env::GetInstance()->last_mouse_location())
             .id();
}

DisplayManagerTestApi::DisplayManagerTestApi()
    : display_manager_(Shell::GetInstance()->display_manager()) {}

DisplayManagerTestApi::~DisplayManagerTestApi() {}

void DisplayManagerTestApi::UpdateDisplay(
    const std::string& display_specs) {
  std::vector<DisplayInfo> display_info_list =
      CreateDisplayInfoListFromString(display_specs, display_manager_);
  bool is_host_origin_set = false;
  for (size_t i = 0; i < display_info_list.size(); ++i) {
    const DisplayInfo& display_info = display_info_list[i];
    if (display_info.bounds_in_native().origin() != gfx::Point(0, 0)) {
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
      gfx::Rect bounds(iter->bounds_in_native().size());
      bounds.set_x(1);
      bounds.set_y(next_y);
      next_y += bounds.height();
      iter->SetBounds(bounds);
    }
  }

  display_manager_->OnNativeDisplaysChanged(display_info_list);
  display_manager_->UpdateInternalDisplayModeListForTest();
  display_manager_->RunPendingTasksForTest();
}

int64_t DisplayManagerTestApi::SetFirstDisplayAsInternalDisplay() {
  const gfx::Display& internal = display_manager_->active_display_list_[0];
  SetInternalDisplayId(internal.id());
  return gfx::Display::InternalDisplayId();
}

void DisplayManagerTestApi::SetInternalDisplayId(int64_t id) {
  gfx::Display::SetInternalDisplayId(id);
  display_manager_->UpdateInternalDisplayModeListForTest();
}

void DisplayManagerTestApi::DisableChangeDisplayUponHostResize() {
  display_manager_->set_change_display_upon_host_resize(false);
}

void DisplayManagerTestApi::SetAvailableColorProfiles(
    int64_t display_id,
    const std::vector<ui::ColorCalibrationProfile>& profiles) {
  display_manager_->display_info_[display_id].set_available_color_profiles(
      profiles);
}

ScopedDisable125DSFForUIScaling::ScopedDisable125DSFForUIScaling() {
  DisplayInfo::SetUse125DSFForUIScalingForTest(false);
}

ScopedDisable125DSFForUIScaling::~ScopedDisable125DSFForUIScaling() {
  DisplayInfo::SetUse125DSFForUIScalingForTest(true);
}

ScopedSetInternalDisplayId::ScopedSetInternalDisplayId(int64_t id) {
  DisplayManagerTestApi().SetInternalDisplayId(id);
}

ScopedSetInternalDisplayId::~ScopedSetInternalDisplayId() {
  gfx::Display::SetInternalDisplayId(gfx::Display::kInvalidDisplayID);
}

bool SetDisplayResolution(int64_t display_id, const gfx::Size& resolution) {
  DisplayManager* display_manager = Shell::GetInstance()->display_manager();
  const DisplayInfo& info = display_manager->GetDisplayInfo(display_id);
  DisplayMode mode;
  if (!GetDisplayModeForResolution(info, resolution, &mode))
    return false;
  return display_manager->SetDisplayMode(display_id, mode);
}

void SwapPrimaryDisplay() {
  if (gfx::Screen::GetScreen()->GetNumDisplays() <= 1)
    return;
  Shell::GetInstance()->window_tree_host_manager()->SetPrimaryDisplay(
      ScreenUtil::GetSecondaryDisplay());
}

DisplayLayout CreateDisplayLayout(DisplayPlacement::Position position,
                                  int offset) {
  DisplayManager* display_manager = Shell::GetInstance()->display_manager();
  DisplayIdList list = display_manager->GetCurrentDisplayIdList();

  DisplayLayout layout;
  layout.primary_id = gfx::Screen::GetScreen()->GetPrimaryDisplay().id();
  layout.placement.position = position;
  layout.placement.offset = offset;
  if (list[0] == layout.primary_id) {
    layout.placement.display_id = list[1];
    layout.placement.parent_display_id = list[0];
  } else {
    layout.placement.display_id = list[0];
    layout.placement.parent_display_id = list[1];
  }
  return layout;
}

}  // namespace test
}  // namespace ash
