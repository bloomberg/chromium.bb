// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DESKTOP_BACKGROUND_DESKTOP_BACKGROUND_CONTROLLER_OBSERVER_H_
#define ASH_DESKTOP_BACKGROUND_DESKTOP_BACKGROUND_CONTROLLER_OBSERVER_H_

#include "ash/ash_export.h"

namespace ash {

class ASH_EXPORT DesktopBackgroundControllerObserver {
 public:
  // Invoked when the wallpaper data is changed.
  virtual void OnWallpaperDataChanged() = 0;

 protected:
  virtual ~DesktopBackgroundControllerObserver() {}
};

}  // namespace ash

#endif  // ASH_DESKTOP_BACKGROUND_DESKTOP_BACKGROUND_CONTROLLER_OBSERVER_H_
