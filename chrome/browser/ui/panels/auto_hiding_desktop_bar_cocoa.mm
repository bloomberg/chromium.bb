// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/auto_hiding_desktop_bar.h"

#include "base/compiler_specific.h"
#include "base/logging.h"

namespace {

class AutoHidingDesktopBarCocoa : public AutoHidingDesktopBar {
 public:
  explicit AutoHidingDesktopBarCocoa(Observer* observer);
  virtual ~AutoHidingDesktopBarCocoa() { }

  // Overridden from AutoHidingDesktopBar:
  virtual void UpdateWorkArea(const gfx::Rect& work_area) OVERRIDE;
  virtual bool IsEnabled(Alignment alignment) OVERRIDE;
  virtual int GetThickness(Alignment alignment) const OVERRIDE;
  virtual Visibility GetVisibility(Alignment alignment) const OVERRIDE;

 private:
  Observer* observer_;

  DISALLOW_COPY_AND_ASSIGN(AutoHidingDesktopBarCocoa);
};

AutoHidingDesktopBarCocoa::AutoHidingDesktopBarCocoa(Observer* observer)
    : observer_(observer) {
  DCHECK(observer);
}

void AutoHidingDesktopBarCocoa::UpdateWorkArea(const gfx::Rect& work_area) {
  // TODO(jianli): Implement.
}

bool AutoHidingDesktopBarCocoa::IsEnabled(
    AutoHidingDesktopBar::Alignment alignment) {
  // TODO(jianli): Implement.
  return false;
}

int AutoHidingDesktopBarCocoa::GetThickness(
    AutoHidingDesktopBar::Alignment alignment) const {
  NOTIMPLEMENTED();
  return 0;
}

AutoHidingDesktopBar::Visibility AutoHidingDesktopBarCocoa::GetVisibility(
    AutoHidingDesktopBar::Alignment alignment) const {
  NOTIMPLEMENTED();
  return AutoHidingDesktopBar::HIDDEN;
}

}  // namespace

// static
AutoHidingDesktopBar* AutoHidingDesktopBar::Create(Observer* observer) {
  return new AutoHidingDesktopBarCocoa(observer);
}
