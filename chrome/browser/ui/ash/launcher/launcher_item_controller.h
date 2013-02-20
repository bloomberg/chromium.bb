// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LAUNCHER_LAUNCHER_ITEM_CONTROLLER_H_
#define CHROME_BROWSER_UI_ASH_LAUNCHER_LAUNCHER_ITEM_CONTROLLER_H_

#include "ash/launcher/launcher_types.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_vector.h"
#include "base/string16.h"

class ChromeLauncherController;
class ChromeLauncherAppMenuItem;

typedef ScopedVector<ChromeLauncherAppMenuItem> ChromeLauncherAppMenuItems;

namespace aura {
class Window;
}

namespace content {
class WebContents;
}

// LauncherItemController is used by ChromeLauncherController to track one
// or more windows associated with a launcher item.
class LauncherItemController {
 public:
  enum Type {
    TYPE_APP,
    TYPE_APP_PANEL,
    TYPE_EXTENSION_PANEL,
    TYPE_SHORTCUT,
    TYPE_TABBED
  };

  LauncherItemController(Type type,
                         const std::string& app_id,
                         ChromeLauncherController* launcher_controller);
  virtual ~LauncherItemController();

  Type type() const { return type_; }
  ash::LauncherID launcher_id() const { return launcher_id_; }
  void set_launcher_id(ash::LauncherID id) { launcher_id_ = id; }
  const std::string& app_id() const { return app_id_; }
  ChromeLauncherController* launcher_controller() {
    return launcher_controller_;
  }

  // Returns the title for this item.
  virtual string16 GetTitle() = 0;

  // Returns true if this item controls |window|.
  virtual bool HasWindow(aura::Window* window) const = 0;

  // Returns true if this item is open.
  virtual bool IsOpen() const = 0;

  // Launches a new instance of the app associated with this item.
  virtual void Launch(int event_flags) = 0;

  // Shows and activates the most-recently-active window associated with the
  // item, or launches the item if it is not currently open.
  virtual void Activate() = 0;

  // Closes all windows associated with this item.
  virtual void Close() = 0;

  // Indicates that the item at |index| has changed from its previous value.
  virtual void LauncherItemChanged(int model_index,
                                   const ash::LauncherItem& old_item) = 0;

  // Called when the item is clicked. The behavior varies by the number of
  // windows associated with the item:
  // * One window: toggles the minimize state.
  // * Multiple windows: cycles the active window.
  // The |event| is dispatched by a view, therefore the type of the
  // event's target is |views::View|.
  virtual void Clicked(const ui::Event& event) = 0;

  // Called when the controlled item is removed from the launcher.
  virtual void OnRemoved() = 0;

  // Called to retrieve the list of running applications.
  virtual ChromeLauncherAppMenuItems GetApplicationList() = 0;

  // Helper function to get the ash::LauncherItemType for the item type.
  ash::LauncherItemType GetLauncherItemType() const;

 protected:
  // Helper function to return the title associated with |app_id_|.
  // Returns an empty title if no matching extension can be found.
  string16 GetAppTitle() const;

 private:
  const Type type_;
  // App id will be empty if there is no app associated with the window.
  const std::string app_id_;
  ash::LauncherID launcher_id_;
  ChromeLauncherController* launcher_controller_;

  DISALLOW_COPY_AND_ASSIGN(LauncherItemController);
};

#endif  // CHROME_BROWSER_UI_ASH_LAUNCHER_LAUNCHER_ITEM_CONTROLLER_H_
