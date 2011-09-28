// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/auto_hiding_desktop_bar.h"

#include "base/compiler_specific.h"
#include "base/logging.h"

namespace {

class AutoHidingDesktopBarGtk : public AutoHidingDesktopBar {
 public:
  explicit AutoHidingDesktopBarGtk(Observer* observer);
  virtual ~AutoHidingDesktopBarGtk() { }

  // Overridden from AutoHidingDesktopBar:
  virtual void UpdateWorkArea(const gfx::Rect& work_area) OVERRIDE;
  virtual bool IsEnabled(Alignment alignment) OVERRIDE;
  virtual int GetThickness(Alignment alignment) const OVERRIDE;
  virtual Visibility GetVisibility(Alignment alignment) const OVERRIDE;

 private:
  Observer* observer_;

  DISALLOW_COPY_AND_ASSIGN(AutoHidingDesktopBarGtk);
};

AutoHidingDesktopBarGtk::AutoHidingDesktopBarGtk(Observer* observer)
    : observer_(observer) {
  DCHECK(observer);
}

void AutoHidingDesktopBarGtk::UpdateWorkArea(const gfx::Rect& work_area) {
  // TODO(prasadt): Not implemented yet.
}

bool AutoHidingDesktopBarGtk::IsEnabled(
    AutoHidingDesktopBar::Alignment alignment) {
  // TODO(prasadt): Not implemented yet.
  return false;
}

int AutoHidingDesktopBarGtk::GetThickness(
    AutoHidingDesktopBar::Alignment alignment) const {
  // TODO(prasadt): Not implemented yet.
  return 0;
}

AutoHidingDesktopBar::Visibility AutoHidingDesktopBarGtk::GetVisibility(
    AutoHidingDesktopBar::Alignment alignment) const {
  // TODO(prasadt): Not implemented yet.
  return AutoHidingDesktopBar::HIDDEN;
}

}  // namespace

// static
AutoHidingDesktopBar* AutoHidingDesktopBar::Create(Observer* observer) {
  return new AutoHidingDesktopBarGtk(observer);
}
