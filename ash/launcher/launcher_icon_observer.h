// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LAUNCHER_LAUNCHER_ICON_OBSERVER_H_
#define ASH_LAUNCHER_LAUNCHER_ICON_OBSERVER_H_
#pragma once

#include "ash/ash_export.h"
#include "base/basictypes.h"

namespace ash {

class ASH_EXPORT LauncherIconObserver {
 public:
  // Invoked when any icon on launcher changes position.
  virtual void OnLauncherIconPositionsChanged() = 0;

 protected:
  virtual ~LauncherIconObserver() {}
};

}  // namespace ash

#endif  // ASH_LAUNCHER_LAUNCHER_ICON_OBSERVER_H_
