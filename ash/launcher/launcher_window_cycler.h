// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LAUNCHER_LAUNCHER_WINDOW_CYCLER_H_
#define ASH_LAUNCHER_LAUNCHER_WINDOW_CYCLER_H_
#pragma once

#include "ash/ash_export.h"
#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"

namespace ash {

class WindowCycleList;

// LauncherWindowCycler is used when the user clicks the browser shortcut button
// in the launcher. If there are no windows to cycle through a new one is
// created (by way of ShellDelegate::CreateNewWindow), otherwise it cycles
// through the existing windows until Reset() is invoked.
class ASH_EXPORT LauncherWindowCycler {
 public:
  LauncherWindowCycler();
  ~LauncherWindowCycler();

  // Invoked each time the button is pressed on the browser shortcut. If not
  // already cycling starts cycling (possibly creating a new window), otherwise
  // cycles to the next window.
  void Cycle();

  // Resets the cycle order so that the next time Cycle() is invoked the set of
  // windows to cycle through is refreshed.
  void Reset();

 private:
  scoped_ptr<WindowCycleList> windows_;

  DISALLOW_COPY_AND_ASSIGN(LauncherWindowCycler);
};

}  // namespace ash

#endif  // ASH_LAUNCHER_LAUNCHER_WINDOW_CYCLER_H_
