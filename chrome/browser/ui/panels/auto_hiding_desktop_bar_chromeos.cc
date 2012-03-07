// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/auto_hiding_desktop_bar.h"

#include "base/compiler_specific.h"

namespace {

class AutoHidingDesktopBarChromeOS : public AutoHidingDesktopBar {
 public:
  explicit AutoHidingDesktopBarChromeOS(Observer* observer);
  virtual ~AutoHidingDesktopBarChromeOS() { }

  // Overridden from AutoHidingDesktopBar:
  virtual void UpdateWorkArea(const gfx::Rect& work_area) OVERRIDE;
  virtual bool IsEnabled(Alignment alignment) OVERRIDE;
  virtual int GetThickness(Alignment alignment) const OVERRIDE;
  virtual Visibility GetVisibility(Alignment alignment) const OVERRIDE;

 private:
  Observer* observer_;

  DISALLOW_COPY_AND_ASSIGN(AutoHidingDesktopBarChromeOS);
};

AutoHidingDesktopBarChromeOS::AutoHidingDesktopBarChromeOS(Observer* observer)
    : observer_(observer) {
}

void AutoHidingDesktopBarChromeOS::UpdateWorkArea(const gfx::Rect& work_area) {
}

bool AutoHidingDesktopBarChromeOS::IsEnabled(
    AutoHidingDesktopBar::Alignment alignment) {
  // No taskbar exists on ChromeOS.
  return false;
}

int AutoHidingDesktopBarChromeOS::GetThickness(
    AutoHidingDesktopBar::Alignment alignment) const {
  return 0;
}

AutoHidingDesktopBar::Visibility AutoHidingDesktopBarChromeOS::GetVisibility(
    AutoHidingDesktopBar::Alignment alignment) const {
  return AutoHidingDesktopBar::HIDDEN;
}

}  // namespace

// static
AutoHidingDesktopBar* AutoHidingDesktopBar::Create(Observer* observer) {
  return new AutoHidingDesktopBarChromeOS(observer);
}
