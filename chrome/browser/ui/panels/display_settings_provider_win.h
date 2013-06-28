// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PANELS_DISPLAY_SETTINGS_PROVIDER_WIN_H_
#define CHROME_BROWSER_UI_PANELS_DISPLAY_SETTINGS_PROVIDER_WIN_H_

#include "chrome/browser/ui/panels/display_settings_provider.h"

#include <windows.h>
#include "base/compiler_specific.h"
#include "base/timer/timer.h"

class DisplaySettingsProviderWin : public DisplaySettingsProvider {
 public:
  DisplaySettingsProviderWin();
  virtual ~DisplaySettingsProviderWin();

 protected:
  // Overridden from DisplaySettingsProvider:
  virtual void OnDisplaySettingsChanged() OVERRIDE;
  virtual bool IsAutoHidingDesktopBarEnabled(
      DesktopBarAlignment alignment) OVERRIDE;
  virtual int GetDesktopBarThickness(
      DesktopBarAlignment alignment) const OVERRIDE;
  virtual DesktopBarVisibility GetDesktopBarVisibility(
      DesktopBarAlignment alignment) const OVERRIDE;

  int GetDesktopBarThicknessFromBounds(
      DesktopBarAlignment alignment, const gfx::Rect& taskbar_bounds) const;
  DesktopBarVisibility GetDesktopBarVisibilityFromBounds(
      DesktopBarAlignment alignment, const gfx::Rect& taskbar_bounds) const;

 private:
  struct Taskbar {
    HWND window;
    DesktopBarVisibility visibility;
    int thickness;
  };

  // Callback to perform periodic check for taskbar changes.
  void OnPollingTimer();

  // Returns true if there is at least one auto-hiding taskbar found.
  bool CheckTaskbars(bool notify_observer);

  gfx::Rect GetBounds(DesktopBarAlignment alignment) const;

  // Maximum number of taskbars we're interested in: bottom, left, and right.
  static const int kMaxTaskbars = 3;

  HMONITOR monitor_;
  Taskbar taskbars_[kMaxTaskbars];
  base::RepeatingTimer<DisplaySettingsProviderWin> polling_timer_;

  DISALLOW_COPY_AND_ASSIGN(DisplaySettingsProviderWin);
};

#endif  // CHROME_BROWSER_UI_PANELS_DISPLAY_SETTINGS_PROVIDER_WIN_H_
