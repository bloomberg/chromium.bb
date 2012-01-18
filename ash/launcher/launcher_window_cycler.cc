// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/launcher/launcher_window_cycler.h"

#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/wm/window_cycle_controller.h"
#include "ash/wm/window_cycle_list.h"

namespace ash {

LauncherWindowCycler::LauncherWindowCycler() {
}

LauncherWindowCycler::~LauncherWindowCycler() {
}

void LauncherWindowCycler::Cycle() {
  if (!WindowCycleController::CanCycle())
    return;

  if (!windows_.get()) {
    windows_.reset(new WindowCycleList(
        ash::Shell::GetInstance()->delegate()->GetCycleWindowList(
            ShellDelegate::SOURCE_LAUNCHER, ShellDelegate::ORDER_MRU)));
  }
  if (windows_->empty())
    ash::Shell::GetInstance()->delegate()->CreateNewWindow();
  else
    windows_->Step(WindowCycleList::FORWARD);
}

void LauncherWindowCycler::Reset() {
  // TODO(sky): If the user cycled more than one time we've now messed up the
  // cycle order. We really need a way to reset it.
  windows_.reset();
}

}  // namespace ash
