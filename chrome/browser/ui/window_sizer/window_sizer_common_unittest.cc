// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/window_sizer/window_sizer_common_unittest.h"

#include "ash/wm/window_resizer.h"
#include "base/compiler_specific.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebKit.h"

TestMonitorInfoProvider::TestMonitorInfoProvider() {};

TestMonitorInfoProvider::~TestMonitorInfoProvider() {};

void TestMonitorInfoProvider::AddMonitor(const gfx::Rect& bounds,
                                         const gfx::Rect& work_area) {
  DCHECK(bounds.Contains(work_area));
  monitor_bounds_.push_back(bounds);
  work_areas_.push_back(work_area);
}

// Overridden from WindowSizer::MonitorInfoProvider:
gfx::Rect TestMonitorInfoProvider::GetPrimaryDisplayWorkArea() const {
  return work_areas_[0];
}

gfx::Rect TestMonitorInfoProvider::GetPrimaryDisplayBounds() const {
  return monitor_bounds_[0];
}

gfx::Rect TestMonitorInfoProvider::GetMonitorWorkAreaMatching(
    const gfx::Rect& match_rect) const {
  return work_areas_[GetMonitorIndexMatchingBounds(match_rect)];
}

size_t TestMonitorInfoProvider::GetMonitorIndexMatchingBounds(
    const gfx::Rect& match_rect) const {
  int max_area = 0;
  size_t max_area_index = 0;
  // Loop through all the monitors, finding the one that intersects the
  // largest area of the supplied match rect.
  for (size_t i = 0; i < work_areas_.size(); ++i) {
    gfx::Rect overlap(match_rect.Intersect(work_areas_[i]));
    int area = overlap.width() * overlap.height();
    if (area > max_area) {
      max_area = area;
      max_area_index = i;
    }
  }
  return max_area_index;
}

TestStateProvider::TestStateProvider():
    has_persistent_data_(false),
    persistent_show_state_(ui::SHOW_STATE_DEFAULT),
    has_last_active_data_(false),
    last_active_show_state_(ui::SHOW_STATE_DEFAULT) {
}

void TestStateProvider::SetPersistentState(const gfx::Rect& bounds,
                                           const gfx::Rect& work_area,
                                           ui::WindowShowState show_state,
                                           bool has_persistent_data) {
  persistent_bounds_ = bounds;
  persistent_work_area_ = work_area;
  persistent_show_state_ = show_state;
  has_persistent_data_ = has_persistent_data;
}

void TestStateProvider::SetLastActiveState(const gfx::Rect& bounds,
                                           ui::WindowShowState show_state,
                                           bool has_last_active_data) {
  last_active_bounds_ = bounds;
  last_active_show_state_ = show_state;
  has_last_active_data_ = has_last_active_data;
}

bool TestStateProvider::GetPersistentState(
    gfx::Rect* bounds,
    gfx::Rect* saved_work_area,
    ui::WindowShowState* show_state) const {
  DCHECK(show_state);
  *bounds = persistent_bounds_;
  *saved_work_area = persistent_work_area_;
  if (*show_state == ui::SHOW_STATE_DEFAULT)
    *show_state = persistent_show_state_;
  return has_persistent_data_;
}

bool TestStateProvider::GetLastActiveWindowState(
    gfx::Rect* bounds,
    ui::WindowShowState* show_state) const {
  DCHECK(show_state);
  *bounds = last_active_bounds_;
  if (*show_state == ui::SHOW_STATE_DEFAULT)
    *show_state = last_active_show_state_;
  return has_last_active_data_;
}

int kWindowTilePixels = WindowSizer::kWindowTilePixels;

// The window sizer commonly used test functions.
void GetWindowBoundsAndShowState(
    const gfx::Rect& monitor1_bounds,
    const gfx::Rect& monitor1_work_area,
    const gfx::Rect& monitor2_bounds,
    const gfx::Rect& bounds,
    const gfx::Rect& work_area,
    ui::WindowShowState show_state_persisted,
    ui::WindowShowState show_state_last,
    Source source,
    const Browser* browser,
    const gfx::Rect& passed_in,
    gfx::Rect* out_bounds,
    ui::WindowShowState* out_show_state) {
  DCHECK(out_show_state);
  TestMonitorInfoProvider* mip = new TestMonitorInfoProvider;
  mip->AddMonitor(monitor1_bounds, monitor1_work_area);
  if (!monitor2_bounds.IsEmpty())
    mip->AddMonitor(monitor2_bounds, monitor2_bounds);
  TestStateProvider* sp = new TestStateProvider;
  if (source == PERSISTED || source == BOTH)
    sp->SetPersistentState(bounds, work_area, show_state_persisted, true);
  if (source == LAST_ACTIVE || source == BOTH)
    sp->SetLastActiveState(bounds, show_state_last, true);

  WindowSizer sizer(sp, mip, browser);
  sizer.DetermineWindowBoundsAndShowState(passed_in,
                                          out_bounds,
                                          out_show_state);
}

void GetWindowBounds(const gfx::Rect& monitor1_bounds,
                            const gfx::Rect& monitor1_work_area,
                            const gfx::Rect& monitor2_bounds,
                            const gfx::Rect& bounds,
                            const gfx::Rect& work_area,
                            Source source,
                            gfx::Rect* out_bounds,
                            const Browser* browser,
                            const gfx::Rect& passed_in) {
  ui::WindowShowState out_show_state = ui::SHOW_STATE_DEFAULT;
  GetWindowBoundsAndShowState(
      monitor1_bounds, monitor1_work_area, monitor2_bounds, bounds, work_area,
      ui::SHOW_STATE_DEFAULT, ui::SHOW_STATE_DEFAULT, source, browser,
      passed_in, out_bounds, &out_show_state);
}

ui::WindowShowState GetWindowShowState(
    ui::WindowShowState show_state_persisted,
    ui::WindowShowState show_state_last,
    Source source,
    const Browser* browser) {
  gfx::Rect bounds = tentwentyfour;
  gfx::Rect work_area = tentwentyfour;
  TestMonitorInfoProvider* mip = new TestMonitorInfoProvider;
  mip->AddMonitor(tentwentyfour, tentwentyfour);
  TestStateProvider* sp = new TestStateProvider;
  if (source == PERSISTED || source == BOTH)
    sp->SetPersistentState(bounds, work_area, show_state_persisted, true);
  if (source == LAST_ACTIVE || source == BOTH)
    sp->SetLastActiveState(bounds, show_state_last, true);

  WindowSizer sizer(sp, mip, browser);

  ui::WindowShowState out_show_state = ui::SHOW_STATE_DEFAULT;
  gfx::Rect out_bounds;
  sizer.DetermineWindowBoundsAndShowState(
      gfx::Rect(),
      &out_bounds,
      &out_show_state);
  return out_show_state;
}
