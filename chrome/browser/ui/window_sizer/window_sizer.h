// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WINDOW_SIZER_WINDOW_SIZER_H_
#define CHROME_BROWSER_UI_WINDOW_SIZER_WINDOW_SIZER_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/host_desktop.h"
#include "ui/gfx/rect.h"

class Browser;

// An interface implemented by an object that can retrieve information about
// the monitors on the system.
class MonitorInfoProvider {
 public:
  virtual ~MonitorInfoProvider() {}

  // Returns the bounds of the work area of the primary monitor.
  virtual gfx::Rect GetPrimaryDisplayWorkArea() const = 0;

  // Returns the bounds of the primary monitor.
  virtual gfx::Rect GetPrimaryDisplayBounds() const = 0;

  // Returns the bounds of the work area of the monitor that most closely
  // intersects the provided bounds.
  virtual gfx::Rect GetMonitorWorkAreaMatching(
      const gfx::Rect& match_rect) const = 0;
};

///////////////////////////////////////////////////////////////////////////////
// WindowSizer
//
//  A class that determines the best new size and position for a window to be
//  shown at based several factors, including the position and size of the last
//  window of the same type, the last saved bounds of the window from the
//  previous session, and default system metrics if neither of the above two
//  conditions exist. The system has built-in providers for monitor metrics
//  and persistent storage (using preferences) but can be overrided with mocks
//  for testing.
//
class WindowSizer {
 public:
  class StateProvider;

  // WindowSizer owns |state_provider| and will create a default
  // MonitorInfoProvider using the physical screen.
  WindowSizer(StateProvider* state_provider, const Browser* browser);

  // WindowSizer owns |state_provider| and |monitor_info_provider|.
  // It will use the supplied monitor info provider. Used only for testing.
  WindowSizer(StateProvider* state_provider,
              MonitorInfoProvider* monitor_info_provider,
              const Browser* browser);

  virtual ~WindowSizer();

  // An interface implemented by an object that can retrieve state from either a
  // persistent store or an existing window.
  class StateProvider {
   public:
    virtual ~StateProvider() {}

    // Retrieve the persisted bounds of the window. Returns true if there was
    // persisted data to retrieve state information, false otherwise.
    virtual bool GetPersistentState(gfx::Rect* bounds,
                                    gfx::Rect* work_area) const = 0;

    // Retrieve the bounds of the most recent window of the matching type.
    // Returns true if there was a last active window to retrieve state
    // information from, false otherwise.
    virtual bool GetLastActiveWindowState(gfx::Rect* bounds) const = 0;
  };

  // Determines the position and size for a window as it is created. This
  // function uses several strategies to figure out optimal size and placement,
  // first looking for an existing active window, then falling back to persisted
  // data from a previous session, finally utilizing a default
  // algorithm. If |specified_bounds| are non-empty, this value is returned
  // instead. For use only in testing.
  void DetermineWindowBounds(const gfx::Rect& specified_bounds,
                             gfx::Rect* bounds) const;

  // Determines the size, position and maximized state for the browser window.
  // See documentation for DetermineWindowBounds above. Normally,
  // |window_bounds| is calculated by calling GetLastActiveWindowState(). To
  // explicitly specify a particular window to base the bounds on, pass in a
  // non-NULL value for |browser|.
  static void GetBrowserWindowBounds(const std::string& app_name,
                                     const gfx::Rect& specified_bounds,
                                     const Browser* browser,
                                     gfx::Rect* window_bounds);

  // Returns the default origin for popups of the given size.
  static gfx::Point GetDefaultPopupOrigin(const gfx::Size& size,
                                          chrome::HostDesktopType type);

  // The number of pixels which are kept free top, left and right when a window
  // gets positioned to its default location.
  static const int kDesktopBorderSize;

  // Maximum width of a window even if there is more room on the desktop.
  static const int kMaximumWindowWidth;

  // How much horizontal and vertical offset there is between newly
  // opened windows.  This value may be different on each platform.
  static const int kWindowTilePixels;

 private:
  // The edge of the screen to check for out-of-bounds.
  enum Edge { TOP, LEFT, BOTTOM, RIGHT };

  // Gets the size and placement of the last window. Returns true if this data
  // is valid, false if there is no last window and the application should
  // restore saved state from preferences using RestoreWindowPosition.
  bool GetLastWindowBounds(gfx::Rect* bounds) const;

  // Gets the size and placement of the last window in the last session, saved
  // in local state preferences. Returns true if local state exists containing
  // this information, false if this information does not exist and a default
  // size should be used.
  bool GetSavedWindowBounds(gfx::Rect* bounds) const;

  // Gets the default window position and size if there is no last window and
  // no saved window placement in prefs. This function determines the default
  // size based on monitor size, etc.
  void GetDefaultWindowBounds(gfx::Rect* default_bounds) const;
#if defined(USE_ASH)
  void GetDefaultWindowBoundsAsh(gfx::Rect* default_bounds) const;
#endif

  // Adjusts |bounds| to be visible on-screen, biased toward the work area of
  // the monitor containing |other_bounds|.  Despite the name, this doesn't
  // guarantee the bounds are fully contained within this monitor's work rect;
  // it just tried to ensure the edges are visible on _some_ work rect.
  // If |saved_work_area| is non-empty, it is used to determine whether the
  // monitor configuration has changed. If it has, bounds are repositioned and
  // resized if necessary to make them completely contained in the current work
  // area.
  void AdjustBoundsToBeVisibleOnMonitorContaining(
      const gfx::Rect& other_bounds,
      const gfx::Rect& saved_work_area,
      gfx::Rect* bounds) const;

  // Determines the position and size for a window as it gets created. This
  // will be called before DetermineWindowBounds. It will return true when the
  // function was setting the bounds structure to the desired size. Otherwise
  // another algorithm should get used to determine the correct bounds.
  bool GetBoundsOverride(const gfx::Rect& specified_bounds,
                         gfx::Rect* bounds) const;
#if defined(USE_ASH)
  bool GetBoundsOverrideAsh(const gfx::Rect& specified_bounds,
                            gfx::Rect* bounds_in_screen) const;
#endif

  // Providers for persistent storage and monitor metrics.
  scoped_ptr<StateProvider> state_provider_;
  scoped_ptr<MonitorInfoProvider> monitor_info_provider_;

  // Note that this browser handle might be NULL.
  const Browser* browser_;

  DISALLOW_COPY_AND_ASSIGN(WindowSizer);
};

#endif  // CHROME_BROWSER_UI_WINDOW_SIZER_WINDOW_SIZER_H_
