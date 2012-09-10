// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/window_sizer/window_sizer.h"

#include "base/compiler_specific.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/ash_init.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window_state.h"
#include "chrome/common/pref_names.h"
#include "ui/gfx/screen.h"

// Minimum height of the visible part of a window.
const int kMinVisibleHeight = 30;
// Minimum width of the visible part of a window.
const int kMinVisibleWidth = 30;

class DefaultMonitorInfoProvider : public MonitorInfoProvider {
 public:
  // Overridden from MonitorInfoProvider:
  virtual gfx::Rect GetPrimaryDisplayWorkArea() const OVERRIDE {
    return gfx::Screen::GetPrimaryDisplay().work_area();
  }
  virtual gfx::Rect GetPrimaryDisplayBounds() const OVERRIDE {
    return gfx::Screen::GetPrimaryDisplay().bounds();
  }
  virtual gfx::Rect GetMonitorWorkAreaMatching(
      const gfx::Rect& match_rect) const OVERRIDE {
    return gfx::Screen::GetDisplayMatching(match_rect).work_area();
  }
};

///////////////////////////////////////////////////////////////////////////////
// An implementation of WindowSizer::StateProvider that gets the last active
// and persistent state from the browser window and the user's profile.
class DefaultStateProvider : public WindowSizer::StateProvider {
 public:
  DefaultStateProvider(const std::string& app_name, const Browser* browser)
      : app_name_(app_name), browser_(browser) {
  }

  // Overridden from WindowSizer::StateProvider:
  virtual bool GetPersistentState(gfx::Rect* bounds,
                                  gfx::Rect* work_area) const {
    DCHECK(bounds);

    if (!browser_ || !browser_->profile()->GetPrefs())
      return false;

    std::string window_name(chrome::GetWindowPlacementKey(browser_));
    const DictionaryValue* wp_pref =
        browser_->profile()->GetPrefs()->GetDictionary(window_name.c_str());
    int top = 0, left = 0, bottom = 0, right = 0;
    bool has_prefs = wp_pref &&
                     wp_pref->GetInteger("top", &top) &&
                     wp_pref->GetInteger("left", &left) &&
                     wp_pref->GetInteger("bottom", &bottom) &&
                     wp_pref->GetInteger("right", &right);
    bounds->SetRect(left, top, std::max(0, right - left),
                    std::max(0, bottom - top));

    int work_area_top = 0;
    int work_area_left = 0;
    int work_area_bottom = 0;
    int work_area_right = 0;
    if (wp_pref) {
      wp_pref->GetInteger("work_area_top", &work_area_top);
      wp_pref->GetInteger("work_area_left", &work_area_left);
      wp_pref->GetInteger("work_area_bottom", &work_area_bottom);
      wp_pref->GetInteger("work_area_right", &work_area_right);
    }
    work_area->SetRect(work_area_left, work_area_top,
                      std::max(0, work_area_right - work_area_left),
                      std::max(0, work_area_bottom - work_area_top));

    return has_prefs;
  }

  virtual bool GetLastActiveWindowState(gfx::Rect* bounds) const {
    // Applications are always restored with the same position.
    if (!app_name_.empty())
      return false;

    // If a reference browser is set, use its window. Otherwise find last
    // active. Panels are never used as reference browsers as panels are
    // specially positioned.
    BrowserWindow* window = NULL;
    // Window may be null if browser is just starting up.
    if (browser_ && browser_->window() && !browser_->window()->IsPanel()) {
      window = browser_->window();
    } else {
      BrowserList::const_reverse_iterator it = BrowserList::begin_last_active();
      BrowserList::const_reverse_iterator end = BrowserList::end_last_active();
      for (; (it != end); ++it) {
        Browser* last_active = *it;
        if (last_active && last_active->is_type_tabbed()) {
          window = last_active->window();
          DCHECK(window);
          break;
        }
      }
    }

    if (window) {
      *bounds = window->GetRestoredBounds();
      return true;
    }

    return false;
  }

 private:
  std::string app_name_;

  // If set, is used as the reference browser for GetLastActiveWindowState.
  const Browser* browser_;
  DISALLOW_COPY_AND_ASSIGN(DefaultStateProvider);
};

///////////////////////////////////////////////////////////////////////////////
// WindowSizer, public:

// The number of pixels which are kept free top, left and right when a window
// gets positioned to its default location.
// static
const int WindowSizer::kDesktopBorderSize = 16;

// Maximum width of a window even if there is more room on the desktop.
// static
const int WindowSizer::kMaximumWindowWidth = 1100;

WindowSizer::WindowSizer(StateProvider* state_provider, const Browser* browser)
    : state_provider_(state_provider),
      monitor_info_provider_(new DefaultMonitorInfoProvider),
      browser_(browser) {
}

WindowSizer::WindowSizer(StateProvider* state_provider,
                         MonitorInfoProvider* monitor_info_provider,
                         const Browser* browser)
    : state_provider_(state_provider),
      monitor_info_provider_(monitor_info_provider),
      browser_(browser) {
}

WindowSizer::~WindowSizer() {
}

// static
void WindowSizer::GetBrowserWindowBounds(const std::string& app_name,
                                         const gfx::Rect& specified_bounds,
                                         const Browser* browser,
                                         gfx::Rect* window_bounds) {
  const WindowSizer sizer(new DefaultStateProvider(app_name, browser), browser);
  sizer.DetermineWindowBounds(specified_bounds, window_bounds);
}

///////////////////////////////////////////////////////////////////////////////
// WindowSizer, private:

void WindowSizer::DetermineWindowBounds(const gfx::Rect& specified_bounds,
                                        gfx::Rect* bounds) const {
  *bounds = specified_bounds;
  if (bounds->IsEmpty()) {
    if (GetBoundsIgnoringPreviousState(specified_bounds, bounds))
      return;
    // See if there's saved placement information.
    if (!GetLastWindowBounds(bounds)) {
      if (!GetSavedWindowBounds(bounds)) {
        // No saved placement, figure out some sensible default size based on
        // the user's screen size.
        GetDefaultWindowBounds(bounds);
      }
    }
  } else {
    // In case that there was a bound given we need to make sure that it is
    // visible and fits on the screen.
    // Find the size of the work area of the monitor that intersects the bounds
    // of the anchor window. Note: AdjustBoundsToBeVisibleOnMonitorContaining
    // does not exactly what we want: It makes only sure that "a minimal part"
    // is visible on the screen.
    gfx::Rect work_area =
        monitor_info_provider_->GetMonitorWorkAreaMatching(*bounds);
    // Resize so that it fits.
    *bounds = bounds->AdjustToFit(work_area);
  }
}

bool WindowSizer::GetLastWindowBounds(gfx::Rect* bounds) const {
  DCHECK(bounds);
  if (!state_provider_.get() ||
      !state_provider_->GetLastActiveWindowState(bounds))
    return false;
  gfx::Rect last_window_bounds = *bounds;
  bounds->Offset(kWindowTilePixels, kWindowTilePixels);
  AdjustBoundsToBeVisibleOnMonitorContaining(last_window_bounds,
                                             gfx::Rect(),
                                             bounds);
  return true;
}

bool WindowSizer::GetSavedWindowBounds(gfx::Rect* bounds) const {
  DCHECK(bounds);
  gfx::Rect saved_work_area;
  if (!state_provider_.get() ||
      !state_provider_->GetPersistentState(bounds, &saved_work_area))
    return false;
  AdjustBoundsToBeVisibleOnMonitorContaining(*bounds, saved_work_area, bounds);
  return true;
}

void WindowSizer::GetDefaultWindowBounds(gfx::Rect* default_bounds) const {
#if defined(USE_ASH)
  // TODO(beng): insufficient but currently necessary. http://crbug.com/133312
  if (chrome::ShouldOpenAshOnStartup()) {
    GetDefaultWindowBoundsAsh(default_bounds);
    return;
  }
#endif
  DCHECK(default_bounds);
  DCHECK(monitor_info_provider_.get());

  gfx::Rect work_area = monitor_info_provider_->GetPrimaryDisplayWorkArea();

  // The default size is either some reasonably wide width, or if the work
  // area is narrower, then the work area width less some aesthetic padding.
  int default_width = std::min(work_area.width() - 2 * kWindowTilePixels, 1050);
  int default_height = work_area.height() - 2 * kWindowTilePixels;

  // For wider aspect ratio displays at higher resolutions, we might size the
  // window narrower to allow two windows to easily be placed side-by-side.
  gfx::Rect screen_size = monitor_info_provider_->GetPrimaryDisplayBounds();
  double width_to_height =
    static_cast<double>(screen_size.width()) / screen_size.height();

  // The least wide a screen can be to qualify for the halving described above.
  static const int kMinScreenWidthForWindowHalving = 1600;
  // We assume 16:9/10 is a fairly standard indicator of a wide aspect ratio
  // computer display.
  if (((width_to_height * 10) >= 16) &&
      work_area.width() > kMinScreenWidthForWindowHalving) {
    // Halve the work area, subtracting aesthetic padding on either side.
    // The padding is set so that two windows, side by side have
    // kWindowTilePixels between screen edge and each other.
    default_width = static_cast<int>(work_area.width() / 2. -
        1.5 * kWindowTilePixels);
  }
  default_bounds->SetRect(kWindowTilePixels + work_area.x(),
                          kWindowTilePixels + work_area.y(),
                          default_width, default_height);
}

void WindowSizer::AdjustBoundsToBeVisibleOnMonitorContaining(
    const gfx::Rect& other_bounds,
    const gfx::Rect& saved_work_area,
    gfx::Rect* bounds) const {
  DCHECK(bounds);
  DCHECK(monitor_info_provider_.get());

  // Find the size of the work area of the monitor that intersects the bounds
  // of the anchor window.
  gfx::Rect work_area =
      monitor_info_provider_->GetMonitorWorkAreaMatching(other_bounds);

  // If height or width are 0, reset to the default size.
  gfx::Rect default_bounds;
  GetDefaultWindowBounds(&default_bounds);
  if (bounds->height() <= 0)
    bounds->set_height(default_bounds.height());
  if (bounds->width() <= 0)
    bounds->set_width(default_bounds.width());

  // Ensure the minimum height and width.
  bounds->set_height(std::max(kMinVisibleHeight, bounds->height()));
  bounds->set_width(std::max(kMinVisibleWidth, bounds->width()));

  // Ensure that the title bar is not above the work area.
  if (bounds->y() < work_area.y())
    bounds->set_y(work_area.y());

  // Reposition and resize the bounds if the saved_work_area is different from
  // the current work area and the current work area doesn't completely contain
  // the bounds.
  if (!saved_work_area.IsEmpty() &&
      saved_work_area != work_area &&
      !work_area.Contains(*bounds)) {
    bounds->set_width(std::min(bounds->width(), work_area.width()));
    bounds->set_height(std::min(bounds->height(), work_area.height()));
    bounds->set_x(
        std::max(work_area.x(),
                 std::min(bounds->x(), work_area.right() - bounds->width())));
    bounds->set_y(
        std::max(work_area.y(),
                 std::min(bounds->y(), work_area.bottom() - bounds->height())));
  }

#if defined(OS_MACOSX)
  // Limit the maximum height.  On the Mac the sizer is on the
  // bottom-right of the window, and a window cannot be moved "up"
  // past the menubar.  If the window is too tall you'll never be able
  // to shrink it again.  Windows does not have this limitation
  // (e.g. can be resized from the top).
  bounds->set_height(std::min(work_area.height(), bounds->height()));

  // On mac, we want to be aggressive about repositioning windows that are
  // partially offscreen.  If the window is partially offscreen horizontally,
  // move it to be flush with the left edge of the work area.
  if (bounds->x() < work_area.x() || bounds->right() > work_area.right())
    bounds->set_x(work_area.x());

  // If the window is partially offscreen vertically, move it to be flush with
  // the top of the work area.
  if (bounds->y() < work_area.y() || bounds->bottom() > work_area.bottom())
    bounds->set_y(work_area.y());
#else
  // On non-Mac platforms, we are less aggressive about repositioning.  Simply
  // ensure that at least kMinVisibleWidth * kMinVisibleHeight is visible.
  const int min_y = work_area.y() + kMinVisibleHeight - bounds->height();
  const int min_x = work_area.x() + kMinVisibleWidth - bounds->width();
  const int max_y = work_area.bottom() - kMinVisibleHeight;
  const int max_x = work_area.right() - kMinVisibleWidth;
  bounds->set_y(std::max(min_y, std::min(max_y, bounds->y())));
  bounds->set_x(std::max(min_x, std::min(max_x, bounds->x())));
#endif  // defined(OS_MACOSX)
}

bool WindowSizer::GetBoundsIgnoringPreviousState(
    const gfx::Rect& specified_bounds,
    gfx::Rect* bounds) const {
#if defined(USE_ASH)
  // TODO(beng): insufficient but currently necessary. http://crbug.com/133312
  if (chrome::ShouldOpenAshOnStartup())
    return GetBoundsIgnoringPreviousStateAsh(specified_bounds, bounds);
#endif
  return false;
}
