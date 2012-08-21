// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LAUNCHER_LAUNCHER_ITEM_CONTROLLER_H_
#define CHROME_BROWSER_UI_ASH_LAUNCHER_LAUNCHER_ITEM_CONTROLLER_H_

#include "ash/launcher/launcher_types.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/string16.h"

class ChromeLauncherController;

namespace aura {
class Window;
}

namespace content {
class WebContents;
}

// LauncherItemController is used by ChromeLauncherController to track one
// or more windows assoicated with a launcher item.
class LauncherItemController {
 public:
  enum Type {
    TYPE_APP,
    TYPE_APP_PANEL,
    TYPE_EXTENSION_PANEL,
    TYPE_TABBED
  };

  LauncherItemController(Type type,
                         ChromeLauncherController* launcher_controller);
  virtual ~LauncherItemController();

  Type type() const { return type_; }
  ash::LauncherID launcher_id() const { return launcher_id_; }
  void set_launcher_id(ash::LauncherID id) { launcher_id_ = id; }
  ChromeLauncherController* launcher_controller() {
    return launcher_controller_;
  }

  // Returns the title for this item.
  virtual string16 GetTitle() const = 0;
  // Returns true if this item controls |window|.
  virtual bool HasWindow(aura::Window* window) const = 0;
  // Shows and actives the appropriate window associated with this item.
  virtual void Open() = 0;
  // Closes all windows associated with this item.
  virtual void Close() = 0;
  // Called when the item is clicked. The bahavior varies by the number of
  // windows associated with the item:
  // * One window: toggles the minimize state.
  // * Multiple windows: cycles the active window.
  virtual void Clicked() = 0;

 private:
  const Type type_;
  ash::LauncherID launcher_id_;
  ChromeLauncherController* launcher_controller_;

  DISALLOW_COPY_AND_ASSIGN(LauncherItemController);
};

#endif  // CHROME_BROWSER_UI_ASH_LAUNCHER_LAUNCHER_ITEM_CONTROLLER_H_
