// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_FIRST_RUN_FIRST_RUN_HELPER_H_
#define ASH_FIRST_RUN_FIRST_RUN_HELPER_H_

#include "ash/ash_export.h"
#include "base/basictypes.h"

namespace gfx {
class Rect;
}

namespace ash {

// Interface used by first-run tutorial to manipulate and retreive information
// about shell elements.
// All returned coordinates are in screen coordinate system.
class ASH_EXPORT FirstRunHelper {
 public:
  FirstRunHelper();
  virtual ~FirstRunHelper();

  // Opens and closes app list.
  virtual void OpenAppList() = 0;
  virtual void CloseAppList() = 0;

  // Returns bounding rectangle of launcher elements.
  virtual gfx::Rect GetLauncherBounds() = 0;

  // Returns bounds of application list button.
  virtual gfx::Rect GetAppListButtonBounds() = 0;

  // Returns bounds of application list. You must open application list before
  // calling this method.
  virtual gfx::Rect GetAppListBounds() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(FirstRunHelper);
};

}  // namespace ash

#endif  // ASH_FIRST_RUN_FIRST_RUN_HELPER_H_

