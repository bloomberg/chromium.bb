// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/touch_view_controller_delegate.h"

#include "ash/shell.h"
#include "ash/wm/maximize_mode/maximize_mode_controller.h"

namespace chromeos {

TouchViewControllerDelegate::TouchViewControllerDelegate() {
  ash::Shell::GetInstance()->AddShellObserver(this);
}

TouchViewControllerDelegate::~TouchViewControllerDelegate() {
  ash::Shell::GetInstance()->RemoveShellObserver(this);
}

void TouchViewControllerDelegate::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void TouchViewControllerDelegate::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool TouchViewControllerDelegate::IsMaximizeModeEnabled() const {
  return ash::Shell::GetInstance()->maximize_mode_controller()->
      IsMaximizeModeWindowManagerEnabled();
}

void TouchViewControllerDelegate::OnMaximizeModeStarted() {
  FOR_EACH_OBSERVER(Observer, observers_, OnMaximizeModeStarted());
}

void TouchViewControllerDelegate::OnMaximizeModeEnded() {
  FOR_EACH_OBSERVER(Observer, observers_, OnMaximizeModeEnded());
}

}  // namespace chromeos
