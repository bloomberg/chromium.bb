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
    gfx::Rect overlap = work_areas_[i];
    overlap.Intersect(match_rect);
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
                            const Browser* browser,
                            const gfx::Rect& passed_in,
                            gfx::Rect* out_bounds) {
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
    const Browser* browser,
    const gfx::Rect& display_config) {
  gfx::Rect bounds = display_config;
  gfx::Rect work_area = display_config;
  TestMonitorInfoProvider* mip = new TestMonitorInfoProvider;
  mip->AddMonitor(display_config, display_config);
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

#if !defined(OS_MACOSX)
TEST(WindowSizerTestCommon, PersistedWindowOffscreenWithNonAggressiveRepositioning) {
  { // off the left but the minimum visibility condition is barely satisfied
    // without relocaiton.
    gfx::Rect initial_bounds(-470, 50, 500, 400);

    gfx::Rect window_bounds;
    GetWindowBounds(p1024x768, p1024x768, gfx::Rect(),
                    initial_bounds, gfx::Rect(), PERSISTED,
                    NULL, gfx::Rect(), &window_bounds);
    EXPECT_EQ(initial_bounds.ToString(), window_bounds.ToString());
  }

  { // off the left and the minimum visibility condition is satisfied by
    // relocation.
    gfx::Rect window_bounds;
    GetWindowBounds(p1024x768, p1024x768, gfx::Rect(),
                    gfx::Rect(-471, 50, 500, 400), gfx::Rect(), PERSISTED,
                    NULL, gfx::Rect(), &window_bounds);
    EXPECT_EQ(gfx::Rect(-470 /* not -471 */, 50, 500, 400).ToString(),
              window_bounds.ToString());
  }

  { // off the top
    gfx::Rect initial_bounds(50, -370, 500, 400);

    gfx::Rect window_bounds;
    GetWindowBounds(p1024x768, p1024x768, gfx::Rect(),
                    gfx::Rect(50, -370, 500, 400), gfx::Rect(), PERSISTED,
                    NULL, gfx::Rect(), &window_bounds);
    EXPECT_EQ("50,0 500x400", window_bounds.ToString());
  }

  { // off the right but the minimum visibility condition is barely satisified
    // without relocation.
    gfx::Rect initial_bounds(994, 50, 500, 400);

    gfx::Rect window_bounds;
    GetWindowBounds(p1024x768, p1024x768, gfx::Rect(),
                    initial_bounds, gfx::Rect(), PERSISTED,
                    NULL, gfx::Rect(), &window_bounds);
    EXPECT_EQ(initial_bounds.ToString(), window_bounds.ToString());
  }

  { // off the right and the minimum visibility condition is satisified by
    // relocation.
    gfx::Rect window_bounds;
    GetWindowBounds(p1024x768, p1024x768, gfx::Rect(),
                    gfx::Rect(995, 50, 500, 400), gfx::Rect(), PERSISTED,
                    NULL, gfx::Rect(), &window_bounds);
    EXPECT_EQ(gfx::Rect(994 /* not 995 */, 50, 500, 400).ToString(),
              window_bounds.ToString());
  }

  { // off the bottom but the minimum visibility condition is barely satisified
    // without relocation.
    gfx::Rect initial_bounds(50, 738, 500, 400);

    gfx::Rect window_bounds;
    GetWindowBounds(p1024x768, p1024x768, gfx::Rect(),
                    initial_bounds, gfx::Rect(), PERSISTED,
                    NULL, gfx::Rect(), &window_bounds);
    EXPECT_EQ(initial_bounds.ToString(), window_bounds.ToString());
  }

  { // off the bottom and the minimum visibility condition is satisified by
    // relocation.
    gfx::Rect window_bounds;
    GetWindowBounds(p1024x768, p1024x768, gfx::Rect(),
                    gfx::Rect(50, 739, 500, 400), gfx::Rect(), PERSISTED,
                    NULL, gfx::Rect(), &window_bounds);
    EXPECT_EQ(gfx::Rect(50, 738 /* not 739 */, 500, 400).ToString(),
              window_bounds.ToString());
  }

  { // off the topleft
    gfx::Rect window_bounds;
    GetWindowBounds(p1024x768, p1024x768, gfx::Rect(),
                    gfx::Rect(-471, -371, 500, 400), gfx::Rect(), PERSISTED,
                    NULL, gfx::Rect(), &window_bounds);
    EXPECT_EQ(gfx::Rect(-470 /* not -471 */, 0, 500, 400).ToString(),
              window_bounds.ToString());
  }

  { // off the topright and the minimum visibility condition is satisified by
    // relocation.
    gfx::Rect window_bounds;
    GetWindowBounds(p1024x768, p1024x768, gfx::Rect(),
                    gfx::Rect(995, -371, 500, 400), gfx::Rect(), PERSISTED,
                    NULL, gfx::Rect(), &window_bounds);
    EXPECT_EQ(gfx::Rect(994 /* not 995 */, 0, 500, 400).ToString(),
              window_bounds.ToString());
  }

  { // off the bottomleft and the minimum visibility condition is satisified by
    // relocation.
    gfx::Rect window_bounds;
    GetWindowBounds(p1024x768, p1024x768, gfx::Rect(),
                    gfx::Rect(-471, 739, 500, 400), gfx::Rect(), PERSISTED,
                    NULL, gfx::Rect(), &window_bounds);
    EXPECT_EQ(gfx::Rect(-470 /* not -471 */,
                        738 /* not 739 */,
                        500,
                        400).ToString(),
              window_bounds.ToString());
  }

  { // off the bottomright and the minimum visibility condition is satisified by
    // relocation.
    gfx::Rect window_bounds;
    GetWindowBounds(p1024x768, p1024x768, gfx::Rect(),
                    gfx::Rect(995, 739, 500, 400), gfx::Rect(), PERSISTED,
                    NULL, gfx::Rect(), &window_bounds);
    EXPECT_EQ(gfx::Rect(994 /* not 995 */,
                        738 /* not 739 */,
                        500,
                        400).ToString(),
              window_bounds.ToString());
  }

  { // entirely off left
    gfx::Rect window_bounds;
    GetWindowBounds(p1024x768, p1024x768, gfx::Rect(),
                    gfx::Rect(-700, 50, 500, 400), gfx::Rect(), PERSISTED,
                    NULL, gfx::Rect(), &window_bounds);
    EXPECT_EQ(gfx::Rect(-470 /* not -700 */, 50, 500, 400).ToString(),
              window_bounds.ToString());
  }

  { // entirely off left (monitor was detached since last run)
    gfx::Rect window_bounds;
    GetWindowBounds(p1024x768, p1024x768, gfx::Rect(),
                    gfx::Rect(-700, 50, 500, 400), left_s1024x768, PERSISTED,
                    NULL, gfx::Rect(), &window_bounds);
    EXPECT_EQ("0,50 500x400", window_bounds.ToString());
  }

  { // entirely off top
    gfx::Rect window_bounds;
    GetWindowBounds(p1024x768, p1024x768, gfx::Rect(),
                    gfx::Rect(50, -500, 500, 400), gfx::Rect(), PERSISTED,
                    NULL, gfx::Rect(), &window_bounds);
    EXPECT_EQ("50,0 500x400", window_bounds.ToString());
  }

  { // entirely off top (monitor was detached since last run)
    gfx::Rect window_bounds;
    GetWindowBounds(p1024x768, p1024x768, gfx::Rect(),
                    gfx::Rect(50, -500, 500, 400), top_s1024x768,
                    PERSISTED, NULL, gfx::Rect(), &window_bounds);
    EXPECT_EQ("50,0 500x400", window_bounds.ToString());
  }

  { // entirely off right
    gfx::Rect window_bounds;
    GetWindowBounds(p1024x768, p1024x768, gfx::Rect(),
                    gfx::Rect(1200, 50, 500, 400), gfx::Rect(), PERSISTED,
                    NULL, gfx::Rect(), &window_bounds);
    EXPECT_EQ(gfx::Rect(994 /* not 1200 */, 50, 500, 400).ToString(),
              window_bounds.ToString());
  }

  { // entirely off right (monitor was detached since last run)
    gfx::Rect window_bounds;
    GetWindowBounds(p1024x768, p1024x768, gfx::Rect(),
                    gfx::Rect(1200, 50, 500, 400), right_s1024x768,
                    PERSISTED, NULL, gfx::Rect(), &window_bounds);
    EXPECT_EQ("524,50 500x400", window_bounds.ToString());
  }

  { // entirely off bottom
    gfx::Rect window_bounds;
    GetWindowBounds(p1024x768, p1024x768, gfx::Rect(),
                    gfx::Rect(50, 800, 500, 400), gfx::Rect(), PERSISTED,
                    NULL, gfx::Rect(), &window_bounds);
    EXPECT_EQ(gfx::Rect(50, 738 /* not 800 */, 500, 400).ToString(),
              window_bounds.ToString());
  }

  { // entirely off bottom (monitor was detached since last run)
    gfx::Rect window_bounds;
    GetWindowBounds(p1024x768, p1024x768, gfx::Rect(),
                    gfx::Rect(50, 800, 500, 400), bottom_s1024x768,
                    PERSISTED, NULL, gfx::Rect(), &window_bounds);
    EXPECT_EQ("50,368 500x400", window_bounds.ToString());
  }
}

// Test that the window is sized appropriately for the first run experience
// where the default window bounds calculation is invoked.
TEST(WindowSizerTestCommon, AdjustFitSize) {
  { // Check that the window gets resized to the screen.
    gfx::Rect window_bounds;
    GetWindowBounds(p1024x768, p1024x768, gfx::Rect(), gfx::Rect(),
                    gfx::Rect(), DEFAULT, NULL,
                    gfx::Rect(-10, -10, 1024 + 20, 768 + 20), &window_bounds);
    EXPECT_EQ("0,0 1024x768", window_bounds.ToString());
  }

  { // Check that a window which hangs out of the screen get moved back in.
    gfx::Rect window_bounds;
    GetWindowBounds(p1024x768, p1024x768, gfx::Rect(), gfx::Rect(),
                    gfx::Rect(), DEFAULT, NULL,
                    gfx::Rect(1020, 700, 100, 100), &window_bounds);
    EXPECT_EQ("924,668 100x100", window_bounds.ToString());
  }
}

#endif // defined(OS_MACOSX)
