// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PANELS_AUTO_HIDING_DESKTOP_BAR_WIN_H_
#define CHROME_BROWSER_UI_PANELS_AUTO_HIDING_DESKTOP_BAR_WIN_H_
#pragma once

#include "chrome/browser/ui/panels/auto_hiding_desktop_bar.h"

#include <windows.h>
#include "base/compiler_specific.h"
#include "base/timer.h"
#include "ui/gfx/rect.h"

class AutoHidingDesktopBarWin : public AutoHidingDesktopBar {
 public:
  explicit AutoHidingDesktopBarWin(Observer* observer);
  virtual ~AutoHidingDesktopBarWin();

  // Overridden from AutoHidingDesktopBar:
  virtual void UpdateWorkArea(const gfx::Rect& work_area) OVERRIDE;
  virtual bool IsEnabled(Alignment alignment) OVERRIDE;
  virtual int GetThickness(Alignment alignment) const OVERRIDE;
  virtual Visibility GetVisibility(Alignment alignment) const OVERRIDE;

#ifdef UNIT_TEST
  void set_work_area(const gfx::Rect& work_area) {
    work_area_ = work_area;
  }
#endif

 private:
  friend class AutoHidingDesktopBarWinTest;

  struct Taskbar {
    HWND window;
    AutoHidingDesktopBar::Visibility visibility;
    int thickness;
  };

  // Callback to perform periodic check for taskbar changes.
  void OnPollingTimer();

  // Returns true if there is at least one auto-hiding taskbar found.
  bool CheckTaskbars(bool notify_observer);

  gfx::Rect GetBounds(Alignment alignment) const;
  int GetThicknessFromBounds(
      Alignment alignment, const gfx::Rect& taskbar_bounds) const;
  Visibility GetVisibilityFromBounds(
      Alignment alignment, const gfx::Rect& taskbar_bounds) const;

  // Maximum number of taskbars we're interested in: bottom, left, and right.
  static const int kMaxTaskbars = 3;

  Observer* observer_;
  gfx::Rect work_area_;
  HMONITOR monitor_;
  Taskbar taskbars_[kMaxTaskbars];
  base::RepeatingTimer<AutoHidingDesktopBarWin> polling_timer_;

  DISALLOW_COPY_AND_ASSIGN(AutoHidingDesktopBarWin);
};

#endif  // CHROME_BROWSER_UI_PANELS_AUTO_HIDING_DESKTOP_BAR_WIN_H_
