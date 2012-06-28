// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WINDOW_SIZER_WINDOW_SIZER_COMMON_UNITTEST_H_
#define CHROME_BROWSER_UI_WINDOW_SIZER_WINDOW_SIZER_COMMON_UNITTEST_H_
#pragma once

// Some standard monitor sizes (no task bar).
static const gfx::Rect tentwentyfour(0, 0, 1024, 768);
static const gfx::Rect twelveeighty(0, 0, 1280, 1024);
static const gfx::Rect sixteenhundred(0, 0, 1600, 1200);
static const gfx::Rect sixteeneighty(0, 0, 1680, 1050);
static const gfx::Rect nineteentwenty(0, 0, 1920, 1200);

// Represents a 1024x768 monitor that is not the primary monitor, arranged to
// the immediate left of the primary 1024x768 monitor.
static const gfx::Rect left_nonprimary(-1024, 0, 1024, 768);

// Represents a 1024x768 monitor that is not the primary monitor, arranged to
// the immediate right of the primary 1024x768 monitor.
static const gfx::Rect right_nonprimary(1024, 0, 1024, 768);

// Represents a 1024x768 monitor that is not the primary monitor, arranged to
// the immediate top of the primary 1024x768 monitor.
static const gfx::Rect top_nonprimary(0, -768, 1024, 768);

// Represents a 1024x768 monitor that is not the primary monitor, arranged to
// the immediate bottom of the primary 1024x768 monitor.
static const gfx::Rect bottom_nonprimary(0, 768, 1024, 768);

// The work area for 1024x768 monitors with different taskbar orientations.
static const gfx::Rect taskbar_bottom_work_area(0, 0, 1024, 734);
static const gfx::Rect taskbar_top_work_area(0, 34, 1024, 734);
static const gfx::Rect taskbar_left_work_area(107, 0, 917, 768);
static const gfx::Rect taskbar_right_work_area(0, 0, 917, 768);

static int kWindowTilePixels = WindowSizer::kWindowTilePixels;

// Testing implementation of WindowSizer::MonitorInfoProvider that we can use
// to fake various monitor layouts and sizes.
class TestMonitorInfoProvider : public MonitorInfoProvider {
 public:
  TestMonitorInfoProvider();
  virtual ~TestMonitorInfoProvider();

  void AddMonitor(const gfx::Rect& bounds, const gfx::Rect& work_area);

  // Overridden from WindowSizer::MonitorInfoProvider:
  virtual gfx::Rect GetPrimaryDisplayWorkArea() const OVERRIDE;

  virtual gfx::Rect GetPrimaryDisplayBounds() const OVERRIDE;

  virtual gfx::Rect GetMonitorWorkAreaMatching(
      const gfx::Rect& match_rect) const OVERRIDE;

 private:
  size_t GetMonitorIndexMatchingBounds(const gfx::Rect& match_rect) const;

  std::vector<gfx::Rect> monitor_bounds_;
  std::vector<gfx::Rect> work_areas_;

  DISALLOW_COPY_AND_ASSIGN(TestMonitorInfoProvider);
};

TestMonitorInfoProvider::TestMonitorInfoProvider() {}
TestMonitorInfoProvider::~TestMonitorInfoProvider() {}

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


// Testing implementation of WindowSizer::StateProvider that we use to fake
// persistent storage and existing windows.
class TestStateProvider : public WindowSizer::StateProvider {
 public:
  TestStateProvider();
  virtual ~TestStateProvider();

  void SetPersistentState(const gfx::Rect& bounds,
                          const gfx::Rect& work_area,
                          bool has_persistent_data);
  void SetLastActiveState(const gfx::Rect& bounds, bool has_last_active_data);

  // Overridden from WindowSizer::StateProvider:
  virtual bool GetPersistentState(gfx::Rect* bounds,
                                  gfx::Rect* saved_work_area) const OVERRIDE;
  virtual bool GetLastActiveWindowState(gfx::Rect* bounds) const OVERRIDE;

 private:
  gfx::Rect persistent_bounds_;
  gfx::Rect persistent_work_area_;
  bool has_persistent_data_;

  gfx::Rect last_active_bounds_;
  bool has_last_active_data_;

  DISALLOW_COPY_AND_ASSIGN(TestStateProvider);
};

TestStateProvider::TestStateProvider():
    has_persistent_data_(false),
    has_last_active_data_(false) {
}

TestStateProvider::~TestStateProvider() {}

void TestStateProvider::SetPersistentState(const gfx::Rect& bounds,
                                           const gfx::Rect& work_area,
                                           bool has_persistent_data) {
  persistent_bounds_ = bounds;
  persistent_work_area_ = work_area;
  has_persistent_data_ = has_persistent_data;
}

void TestStateProvider::SetLastActiveState(const gfx::Rect& bounds,
                                           bool has_last_active_data) {
  last_active_bounds_ = bounds;
  has_last_active_data_ = has_last_active_data;
}

bool TestStateProvider::GetPersistentState(gfx::Rect* bounds,
                                           gfx::Rect* saved_work_area) const {
  *bounds = persistent_bounds_;
  *saved_work_area = persistent_work_area_;
  return has_persistent_data_;
}

bool TestStateProvider::GetLastActiveWindowState(gfx::Rect* bounds) const {
  *bounds = last_active_bounds_;
  return has_last_active_data_;
}

// A convenience function to read the window bounds from the window sizer
// according to the specified configuration.
enum Source { DEFAULT, LAST_ACTIVE, PERSISTED };
static void GetWindowBounds(const gfx::Rect& monitor1_bounds,
                            const gfx::Rect& monitor1_work_area,
                            const gfx::Rect& monitor2_bounds,
                            const gfx::Rect& state,
                            const gfx::Rect& work_area,
                            Source source,
                            gfx::Rect* out_bounds,
                            const Browser* browser,
                            const gfx::Rect& passed_in) {
  TestMonitorInfoProvider* mip = new TestMonitorInfoProvider;
  mip->AddMonitor(monitor1_bounds, monitor1_work_area);
  if (!monitor2_bounds.IsEmpty())
    mip->AddMonitor(monitor2_bounds, monitor2_bounds);
  TestStateProvider* sp = new TestStateProvider;
  if (source == PERSISTED)
    sp->SetPersistentState(state, work_area, true);
  else if (source == LAST_ACTIVE)
    sp->SetLastActiveState(state, true);

  WindowSizer sizer(sp, mip, browser);
  sizer.DetermineWindowBounds(passed_in, out_bounds);
}

#endif  // CHROME_BROWSER_UI_WINDOW_SIZER_WINDOW_SIZER_COMMON_UNITTEST_H_
