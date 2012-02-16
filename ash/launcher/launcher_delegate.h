// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LAUNCHER_LAUNCHER_DELEGATE_H_
#define ASH_LAUNCHER_LAUNCHER_DELEGATE_H_
#pragma once

#include "ash/ash_export.h"
#include "base/string16.h"

namespace ash {

struct LauncherItem;

// Delegate for the Launcher.
class ASH_EXPORT LauncherDelegate {
 public:
  virtual ~LauncherDelegate() {}

  // Invoked when the user clicks on button in the launcher to create a new
  // window.
  virtual void CreateNewWindow() = 0;

  // Invoked when the user clicks on a window entry in the launcher.
  virtual void ItemClicked(const LauncherItem& item) = 0;

  // Returns the resource id of the image to show on the browser shortcut
  // button.
  virtual int GetBrowserShortcutResourceId() = 0;

  // Returns the title to display for the specified launcher item.
  virtual string16 GetTitle(const LauncherItem& item) = 0;
};

}  // namespace ash

#endif  // ASH_LAUNCHER_LAUNCHER_DELEGATE_H_
