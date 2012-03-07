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

  // Observer can listen to various events regarding the desktop bar changes.
  class Observer {
   public:
    // Called when any of the desktop bars get their thickness changed.
    // Note that if an auto-hiding desktop bar is moved from one edge
    // to another edge, it will cause thickness changes to both edges.
    virtual void OnAutoHidingDesktopBarThicknessChanged() = 0;

    // Called when an auto-hiding desktop bar has its visibility changed.
    virtual void OnAutoHidingDesktopBarVisibilityChanged(
        DesktopBarAlignment alignment, DesktopBarVisibility visibility) = 0;
  };

  static DisplaySettingsProvider* Create(Observer* observer);

  virtual ~DisplaySettingsProvider();

  // Returns the bounds of the work area.
  virtual gfx::Rect GetWorkArea();

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

#ifdef UNIT_TEST
  void set_work_area(const gfx::Rect& work_area) {
    work_area_ = work_area;
  }
#endif

 protected:
  explicit DisplaySettingsProvider(Observer* observer);

  // Invoked when the work area is changed in order to update the information
  // about the desktop bars. We only care about the desktop bars that sit on
  // the screen that hosts the specified work area.
  virtual void OnWorkAreaChanged();

  Observer* observer_;
  gfx::Rect work_area_;
};

#endif  // CHROME_BROWSER_UI_PANELS_DISPLAY_SETTINGS_PROVIDER_H_
