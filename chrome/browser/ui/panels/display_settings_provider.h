// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PANELS_DISPLAY_SETTINGS_PROVIDER_H_
#define CHROME_BROWSER_UI_PANELS_DISPLAY_SETTINGS_PROVIDER_H_
#pragma once

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

  // Observer can listen to the event regarding the display area change.
  class DisplayAreaObserver {
   public:
    virtual void OnDisplayAreaChanged(const gfx::Rect& display_area) = 0;
  };

  // Observer can listen to the event regarding the desktop bar change.
  class DesktopBarObserver {
   public:
    virtual void OnAutoHidingDesktopBarVisibilityChanged(
        DesktopBarAlignment alignment, DesktopBarVisibility visibility) = 0;
  };

  static DisplaySettingsProvider* Create();

  virtual ~DisplaySettingsProvider();

  // Returns the bounds of the display area.
  gfx::Rect GetDisplayArea();

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

  DisplayAreaObserver* display_area_observer() const {
    return display_area_observer_;
  }
  void set_display_area_observer(DisplayAreaObserver* display_area_observer) {
    display_area_observer_ = display_area_observer;
  }

  DesktopBarObserver* desktop_bar_observer() const {
    return desktop_bar_observer_;
  }
  void set_desktop_bar_observer(DesktopBarObserver* desktop_bar_observer) {
    desktop_bar_observer_ = desktop_bar_observer;
  }

  gfx::Rect work_area() const { return work_area_; }

 protected:
  DisplaySettingsProvider();

  // Returns the bounds of the work area that has not been adjusted to take
  // auto-hiding desktop bars into account. This can be overridden by the
  // testing code.
  virtual gfx::Rect GetWorkArea() const;

  void OnAutoHidingDesktopBarChanged();

 private:
  // Adjusts the work area to exclude the influence of auto-hiding desktop bars.
  void AdjustWorkAreaForAutoHidingDesktopBars();

  DisplayAreaObserver* display_area_observer_;
  DesktopBarObserver* desktop_bar_observer_;

  // The maximum work area avaialble. This area does not include the area taken
  // by the always-visible (non-auto-hiding) desktop bars.
  gfx::Rect work_area_;

  // The useable work area for computing the panel bounds. This area excludes
  // the potential area that could be taken by the auto-hiding desktop
  // bars (we only consider those bars that are aligned to bottom, left, and
  // right of the screen edges) when they become fully visible.
  gfx::Rect adjusted_work_area_;
};

#endif  // CHROME_BROWSER_UI_PANELS_DISPLAY_SETTINGS_PROVIDER_H_
