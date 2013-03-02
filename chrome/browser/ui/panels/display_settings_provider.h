// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PANELS_DISPLAY_SETTINGS_PROVIDER_H_
#define CHROME_BROWSER_UI_PANELS_DISPLAY_SETTINGS_PROVIDER_H_

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

  class DisplayObserver {
   public:
    virtual void OnDisplayChanged() = 0;
  };

  class DesktopBarObserver {
   public:
    virtual void OnAutoHidingDesktopBarVisibilityChanged(
        DesktopBarAlignment alignment, DesktopBarVisibility visibility) = 0;
    virtual void OnAutoHidingDesktopBarThicknessChanged(
        DesktopBarAlignment alignment, int thickness) = 0;
  };

  class FullScreenObserver {
   public:
    virtual void OnFullScreenModeChanged(bool is_full_screen) = 0;
  };

  static DisplaySettingsProvider* Create();

  virtual ~DisplaySettingsProvider();

  // Subscribes/unsubscribes from the display settings change notification.
  void AddDisplayObserver(DisplayObserver* observer);
  void RemoveDisplayObserver(DisplayObserver* observer);

  void AddDesktopBarObserver(DesktopBarObserver* observer);
  void RemoveDesktopBarObserver(DesktopBarObserver* observer);

  void AddFullScreenObserver(FullScreenObserver* observer);
  void RemoveFullScreenObserver(FullScreenObserver* observer);

  //
  // Display Area:
  //   This is the area of a display (monitor). There could be multiple display
  //   areas.
  // Work Area:
  //   This is the standard work area returned by the system. It is usually
  //   computed by the system as the part of display area that excludes
  //   top-most system menu or bars aligned to the screen edges.
  //

  // Returns the bounds of primary display.
  virtual gfx::Rect GetPrimaryDisplayArea() const;

  // Returns the bounds of the work area of primary display.
  virtual gfx::Rect GetPrimaryWorkArea() const;

  // Returns the bounds of the display area that most closely intersects the
  // provided bounds.
  virtual gfx::Rect GetDisplayAreaMatching(const gfx::Rect& bounds) const;

  // Returns the bounds of the work area that most closely intersects the
  // provided bounds.
  virtual gfx::Rect GetWorkAreaMatching(const gfx::Rect& bounds) const;

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

  ObserverList<DisplayObserver>& display_observers() {
    return display_observers_;
  }

  ObserverList<DesktopBarObserver>& desktop_bar_observers() {
    return desktop_bar_observers_;
  }

  ObserverList<FullScreenObserver>& full_screen_observers() {
    return full_screen_observers_;
  }

  bool is_full_screen() const { return is_full_screen_; }

 protected:
  DisplaySettingsProvider();

  // Returns true if we need to perform fullscreen check periodically.
  virtual bool NeedsPeriodicFullScreenCheck() const;

  // Returns true if full screen or presentation mode in main screen is entered.
  virtual bool IsFullScreen() const;

  // Callback to perform periodic check for full screen mode changes.
  void CheckFullScreenMode();

 private:
  // Observers that listen to various display settings changes.
  ObserverList<DisplayObserver> display_observers_;
  ObserverList<DesktopBarObserver> desktop_bar_observers_;
  ObserverList<FullScreenObserver> full_screen_observers_;

  // True if full screen mode or presentation mode is entered.
  bool is_full_screen_;

  // Timer used to detect full-screen mode change.
  base::RepeatingTimer<DisplaySettingsProvider> full_screen_mode_timer_;

  DISALLOW_COPY_AND_ASSIGN(DisplaySettingsProvider);
};

#endif  // CHROME_BROWSER_UI_PANELS_DISPLAY_SETTINGS_PROVIDER_H_
