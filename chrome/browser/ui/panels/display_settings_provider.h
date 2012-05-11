// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PANELS_DISPLAY_SETTINGS_PROVIDER_H_
#define CHROME_BROWSER_UI_PANELS_DISPLAY_SETTINGS_PROVIDER_H_
#pragma once

#include "base/observer_list.h"
#include "base/timer.h"
#include "ui/gfx/rect.h"

// Encapsulates the logic to provide display settings support, including the
// information for:
// 1) Work area
// 2) Auto-hiding desktop bars, like Windows taskbar and MacOSX dock.
class DisplaySettingsProvider {
 public:
  // Indicates which screen edge the desktop bar is aligned to.
  // We do not care about the desktop aligned to the top screen edge.
  enum DesktopBarAlignment {
    DESKTOP_BAR_ALIGNED_BOTTOM = 0,
    DESKTOP_BAR_ALIGNED_LEFT = 1,
    DESKTOP_BAR_ALIGNED_RIGHT = 2
  };

  // Indicates current visibility state of the desktop bar.
  enum DesktopBarVisibility {
    DESKTOP_BAR_VISIBLE,
    DESKTOP_BAR_ANIMATING,
    DESKTOP_BAR_HIDDEN
  };

  class DisplayAreaObserver {
   public:
    virtual void OnDisplayAreaChanged(const gfx::Rect& display_area) = 0;
  };

  class DesktopBarObserver {
   public:
    virtual void OnAutoHidingDesktopBarVisibilityChanged(
        DesktopBarAlignment alignment, DesktopBarVisibility visibility) = 0;
  };

  class FullScreenObserver {
   public:
    virtual void OnFullScreenModeChanged(bool is_full_screen) = 0;
  };

  static DisplaySettingsProvider* Create();

  virtual ~DisplaySettingsProvider();

  // Subscribes/unsubscribes from the display settings change notification.
  void AddDisplayAreaObserver(DisplayAreaObserver* observer);
  void RemoveDisplayAreaObserver(DisplayAreaObserver* observer);

  void AddDesktopBarObserver(DesktopBarObserver* observer);
  void RemoveDesktopBarObserver(DesktopBarObserver* observer);

  void AddFullScreenObserver(FullScreenObserver* observer);
  void RemoveFullScreenObserver(FullScreenObserver* observer);

  //
  // Primary Screen Area:
  //   This is the screen area of the primary monitor.
  // Work Area:
  //   This is the standard work area returned by the system. It is usually
  //   computed by the system as the part of primary screen area that excludes
  //   top-most system menu or bars aligned to the screen edges.
  // Display Area:
  //   This is the area further trimmed down for the purpose of showing panels.
  //   On Windows, we also exclude auto-hiding taskbars. For all other
  //   platforms, it is same as work area.
  //

  // Returns the bounds of the display area.
  gfx::Rect GetDisplayArea();

  // Returns the bounds of the primary screen area. This can be overridden by
  // the testing code.
  virtual gfx::Rect GetPrimaryScreenArea() const;

  // Invoked when the display settings has changed, due to any of the following:
  // 1) screen resolution changes
  // 2) the thickness of desktop bar changes
  // 3) desktop bar switches between auto-hiding and non-auto-hiding
  virtual void OnDisplaySettingsChanged();

  // Returns true if there is a desktop bar that is aligned to the specified
  // screen edge and set to auto-hide.
  virtual bool IsAutoHidingDesktopBarEnabled(DesktopBarAlignment alignment);

  // Returns the thickness of the desktop bar that is aligned to the specified
  // screen edge, when it is visible. When the desktop bar is aligned to bottom
  // edge, this is the height of the bar. If the desktop bar is aligned to
  // left or right edge, this is the width of the bar.
  virtual int GetDesktopBarThickness(DesktopBarAlignment alignment) const;

  // Returns the visibility state of the desktop bar that is aligned to the
  // specified screen edge.
  virtual DesktopBarVisibility GetDesktopBarVisibility(
      DesktopBarAlignment alignment) const;

  ObserverList<DisplayAreaObserver>& display_area_observers() {
    return display_area_observers_;
  }

  ObserverList<DesktopBarObserver>& desktop_bar_observers() {
    return desktop_bar_observers_;
  }

  ObserverList<FullScreenObserver>& full_screen_observers() {
    return full_screen_observers_;
  }

  gfx::Rect work_area() const { return work_area_; }
  bool is_full_screen() const { return is_full_screen_; }

 protected:
  DisplaySettingsProvider();

  // Returns the bounds of the work area that has not been adjusted to take
  // auto-hiding desktop bars into account. This can be overridden by the
  // testing code.
  virtual gfx::Rect GetWorkArea() const;

  // Callback to perform periodic check for full screen mode changes.
  virtual void CheckFullScreenMode();

  void OnAutoHidingDesktopBarChanged();

 private:
  // Adjusts the work area to exclude the influence of auto-hiding desktop bars.
  void AdjustWorkAreaForAutoHidingDesktopBars();

  // Observers that listen to various display settings changes.
  ObserverList<DisplayAreaObserver> display_area_observers_;
  ObserverList<DesktopBarObserver> desktop_bar_observers_;
  ObserverList<FullScreenObserver> full_screen_observers_;

  // The maximum work area avaialble. This area does not include the area taken
  // by the always-visible (non-auto-hiding) desktop bars.
  gfx::Rect work_area_;

  // The useable work area for computing the panel bounds. This area excludes
  // the potential area that could be taken by the auto-hiding desktop
  // bars (we only consider those bars that are aligned to bottom, left, and
  // right of the screen edges) when they become fully visible.
  gfx::Rect adjusted_work_area_;

  // True if full screen mode or presentation mode is entered.
  bool is_full_screen_;

  // Timer used to detect full-screen mode change.
  base::RepeatingTimer<DisplaySettingsProvider> full_screen_mode_timer_;

  DISALLOW_COPY_AND_ASSIGN(DisplaySettingsProvider);
};

#endif  // CHROME_BROWSER_UI_PANELS_DISPLAY_SETTINGS_PROVIDER_H_
