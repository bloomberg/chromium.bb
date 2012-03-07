// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PANELS_AUTO_HIDING_DESKTOP_BAR_H_
#define CHROME_BROWSER_UI_PANELS_AUTO_HIDING_DESKTOP_BAR_H_
#pragma once

#include "base/memory/ref_counted.h"

namespace gfx {
class Rect;
}

// Encapsulates the logic to deal with always-on-top auto-hiding desktop bars,
// like Windows taskbar or MacOSX dock.
// Note that the ref count is needed by using PostTask in the implementation of
// MockAutoHidingDesktopBar that derives from AutoHidingDesktopBar.
class AutoHidingDesktopBar : public base::RefCounted<AutoHidingDesktopBar> {
 public:
  // Indicates which screen edge the desktop bar is aligned to.
  // We do not care about the desktop aligned to the top screen edge.
  enum Alignment {
    ALIGN_BOTTOM = 0,
    ALIGN_LEFT = 1,
    ALIGN_RIGHT = 2
  };

  enum Visibility {
    VISIBLE,
    ANIMATING,
    HIDDEN
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
        Alignment alignment, Visibility visibility) = 0;
  };

  static AutoHidingDesktopBar* Create(Observer* observer);

  virtual ~AutoHidingDesktopBar() { }

  // This should be called each time when the work area is changed. We only
  // care about the desktop bars that sit on the screen that hosts the specified
  // work area.
  virtual void UpdateWorkArea(const gfx::Rect& work_area) = 0;

  // Returns true if there is a desktop bar that is aligned to the specified
  // screen edge and set to auto-hide.
  virtual bool IsEnabled(Alignment alignment) = 0;

  // Returns the thickness of the desktop bar that is aligned to the specified
  // screen edge, when it is visible. When the desktop bar is aligned to bottom
  // edge, this is the height of the bar. If the desktop bar is aligned to
  // left or right edge, this is the width of the bar.
  virtual int GetThickness(Alignment alignment) const = 0;

  // Returns the visibility state of the desktop bar that is aligned to the
  // specified screen edge.
  virtual Visibility GetVisibility(Alignment alignment) const = 0;
};

#endif  // CHROME_BROWSER_UI_PANELS_AUTO_HIDING_DESKTOP_BAR_H_
